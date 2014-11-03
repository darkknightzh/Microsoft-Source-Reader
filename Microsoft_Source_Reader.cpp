/*///////////////////////////////////////////////////////////////////////////
// a simple code of the usage of Microsoft Source Reader
// Welcome to point out the mistakes of the program
// Forgive me for forget the references of the code. 
// Some code comes from : http://msdn.microsoft.com/en-us/library/windows/desktop/dd389281(v=vs.85).aspx
// Meanwhile thanks Microsoft
// Writen by darkknightzh
// 2014.11.3
*////////////////////////////////////////////////////////////////////////////



#include "stdafx.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mftransform.h>
#include <iostream>
#include <strsafe.h>
#include <stdio.h>
#include <mferror.h>   //head file of MF_E_NO_MORE_TYPES

using namespace std;

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "Mfuuid.lib")

template <class T> void SafeRelease(T **ppT);
void saveBMP(unsigned char* data, int num, int bmpWidth, int bmpHeight);

LPCWSTR GetGUIDNameConst(const GUID& guid);
HRESULT GetGUIDName(const GUID& guid, WCHAR **ppwsz);
HRESULT LogAttributeValueByIndex(IMFAttributes *pAttr, DWORD index);
HRESULT SpecialCaseAttributeValue(GUID guid, const PROPVARIANT& var);
void DBGMSG(PCWSTR format, ...);
HRESULT LogMediaType(IMFMediaType *pType);
HRESULT CreateVideoDeviceSource(IMFMediaSource **ppSource);
HRESULT EnumerateCaptureFormats(IMFMediaSource *pSource);
HRESULT SetDeviceFormat(IMFMediaSource *pSource, DWORD dwFormatIndex);
HRESULT ConfigureDecoder(IMFSourceReader *pReader, DWORD dwStreamIndex);
HRESULT ProcessSamples(IMFSourceReader *pReader);
HRESULT OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
	LONGLONG llTimestamp, IMFSample* pSample);

#define IMGWIDTH 1280
#define IMGHEIGHT 960

int _tmain(int argc, _TCHAR* argv[])
{
	IMFMediaSource *ppSource = NULL;
	CreateVideoDeviceSource(&ppSource);

// 	hr = EnumerateCaptureFormats(ppSource);  // This can show the formats the camera support.
// 	if (FAILED(hr))
// 		abort();
	HRESULT hr;
	IMFSourceReader *pReader;
	hr = MFCreateSourceReaderFromMediaSource(ppSource, NULL, &pReader);
	if (FAILED(hr))
		abort();

	hr = SetDeviceFormat(ppSource, 6);   //I need to configure the camera to format 6.
	if (FAILED(hr))
		abort();

	ProcessSamples(pReader);

	SafeRelease(&pReader);
	SafeRelease(&ppSource);
	MFShutdown();
	CoUninitialize();
}

HRESULT ProcessSamples(IMFSourceReader *pReader)
{
	HRESULT hr = S_OK;
	IMFSample *pSample = NULL;
	size_t  cSamples = 0;

	_LARGE_INTEGER time_start;    /*begin time */
	_LARGE_INTEGER time_over;        /*end time*/
	double dqFreq;                /*timer frequence*/
	LARGE_INTEGER f;            /*timer frequence*/
	QueryPerformanceFrequency(&f);
	dqFreq = (double)f.QuadPart;

	QueryPerformanceCounter(&time_start);


	bool quit = false;
	while (!quit)
	{
		DWORD streamIndex, flags;
		LONGLONG llTimeStamp;

		hr = pReader->ReadSample(
			MF_SOURCE_READER_ANY_STREAM,    // Stream index.
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llTimeStamp,                   // Receives the time stamp.
			&pSample                        // Receives the sample or NULL.
			);

		if (FAILED(hr))
			break;

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			wprintf(L"\tEnd of stream\n");
			quit = true;
		}

		if (pSample)
		{
			BYTE* data;
			IMFMediaBuffer* buffer;
			DWORD max, current;

			//		printf(" cSamples = %d\n", cSamples);
			++cSamples;
			pSample->GetBufferByIndex(0, &buffer);
			buffer->Lock(&data, &max, &current);

		//	saveBMP(data, cSamples, IMGWIDTH, IMGHEIGHT);

			buffer->Unlock();
			SafeRelease(&buffer);
		
			QueryPerformanceCounter(&time_over);          //In order to find the frames per second of the camera.
			double usedtime = ((time_over.QuadPart - time_start.QuadPart) / dqFreq);
			if (usedtime>1)
			{
				printf(" cSamples = %d\n", cSamples);
				cSamples = 0;
				QueryPerformanceCounter(&time_start);
			}
		}
		SafeRelease(&pSample);
	}

	SafeRelease(&pSample);
	return hr;
}


HRESULT ConfigureDecoder(IMFSourceReader *pReader, DWORD dwStreamIndex)
{
	IMFMediaType *pNativeType = NULL;
	IMFMediaType *pType = NULL;

	// Find the native format of the stream.
	HRESULT hr = pReader->GetNativeMediaType(dwStreamIndex, 0, &pNativeType);
	if (FAILED(hr))
		return hr;

	GUID majorType, subtype;

	hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);  // Find the major type.
	if (FAILED(hr))
		goto done;

	hr = MFCreateMediaType(&pType);  // Define the output type.
	if (FAILED(hr))
		goto done;

	hr = pType->SetGUID(MF_MT_MAJOR_TYPE, majorType);
	if (FAILED(hr))
		goto done;

	if (majorType == MFMediaType_Video)  // Select a subtype.
		subtype = MFVideoFormat_NV12;
// 	else if (majorType == MFMediaType_Audio)
// 		subtype = MFAudioFormat_PCM;
	else
		goto done;   // Unrecognized type. Skip.

	hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);
	if (FAILED(hr))
		goto done;

	hr = pReader->SetCurrentMediaType(dwStreamIndex, NULL, pType);  // Set the uncompressed format.
	if (FAILED(hr))
		goto done;

done:
	SafeRelease(&pNativeType);
	SafeRelease(&pType);
	return hr;
}

HRESULT CreateVideoDeviceSource(IMFMediaSource **ppSource)
{
	HRESULT hr;
	hr = CoInitialize(NULL);
	if (FAILED(hr))
		abort();
	hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(hr))
		abort();
		
	*ppSource = NULL;

	IMFMediaSource *pSource = NULL;
	IMFAttributes *pAttributes = NULL;
	IMFActivate **ppDevices = NULL;

	// Create an attribute store to specify the enumeration parameters.
	/*HRESULT*/ hr = MFCreateAttributes(&pAttributes, 1);
	if (FAILED(hr))
		abort();

	// Source type: video capture devices
	hr = pAttributes->SetGUID( MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID );
	if (FAILED(hr))
		abort();

	// Enumerate devices.
	UINT32 count;
	hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
	if (FAILED(hr))
		abort();
	if (count == 0)
	{
		hr = E_FAIL;
		return hr;
	}

	// Create the media source object.
	hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
	if (FAILED(hr))
		abort();

	*ppSource = pSource;
	(*ppSource)->AddRef();

	// release part
	SafeRelease(&pAttributes);

	for (DWORD i = 0; i < count; i++)
	{
		SafeRelease(&ppDevices[i]);
	}
	CoTaskMemFree(ppDevices);
	SafeRelease(&pSource);    //此处不确定，是否需要SafeRelease。
	return hr;
}

HRESULT EnumerateCaptureFormats(IMFMediaSource *pSource)
{
	IMFPresentationDescriptor *pPD = NULL;
	IMFStreamDescriptor *pSD = NULL;
	IMFMediaTypeHandler *pHandler = NULL;
	IMFMediaType *pType = NULL;

	HRESULT hr = pSource->CreatePresentationDescriptor(&pPD);
	if (FAILED(hr))
		goto done;

	BOOL fSelected;
	hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);
	if (FAILED(hr))
		goto done;

	hr = pSD->GetMediaTypeHandler(&pHandler);
	if (FAILED(hr))
		goto done;

	DWORD cTypes = 0;
	hr = pHandler->GetMediaTypeCount(&cTypes);
	if (FAILED(hr))
		goto done;

	for (DWORD i = 0; i < cTypes; i++)
	{
		hr = pHandler->GetMediaTypeByIndex(i, &pType);
		if (FAILED(hr))
			goto done;

		LogMediaType(pType);
		OutputDebugString(L"\n");

		SafeRelease(&pType);
	}

done:
	SafeRelease(&pPD);
	SafeRelease(&pSD);
	SafeRelease(&pHandler);
	SafeRelease(&pType);
	return hr;
}

HRESULT LogMediaType(IMFMediaType *pType)
{
	UINT32 count = 0;

	HRESULT hr = pType->GetCount(&count);
	if (FAILED(hr))
		return hr;

	if (count == 0)
		DBGMSG(L"Empty media type.\n");

	for (UINT32 i = 0; i < count; i++)
	{
		hr = LogAttributeValueByIndex(pType, i);
		if (FAILED(hr))
			break;
	}
	return hr;
}

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

HRESULT SetDeviceFormat(IMFMediaSource *pSource, DWORD dwFormatIndex)
{
	// 经EnumerateCaptureFormats，该平板共支持下列分辨率设置。
// 	0   640 x 480  MFVideoFormat_NV12
// 	1   640 x 480  MFVideoFormat_YUY2
// 	2   640 x 360  MFVideoFormat_NV12
// 	3   640 x 360  MFVideoFormat_YUY2
// 	4  1280 x 720  MFVideoFormat_NV12
// 	5  1280 x 720  MFVideoFormat_YUY2
// 	6  1280 x 960  MFVideoFormat_NV12
// 	7  1280 x 960  MFVideoFormat_YUY2

	IMFPresentationDescriptor *pPD = NULL;
	IMFStreamDescriptor *pSD = NULL;
	IMFMediaTypeHandler *pHandler = NULL;
	IMFMediaType *pType = NULL;

	HRESULT hr = pSource->CreatePresentationDescriptor(&pPD);
	if (FAILED(hr))
		goto done;

	BOOL fSelected;
	hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);
	if (FAILED(hr))
		goto done;

	hr = pSD->GetMediaTypeHandler(&pHandler);
	if (FAILED(hr))
		goto done;

	hr = pHandler->GetMediaTypeByIndex(dwFormatIndex, &pType);
	if (FAILED(hr))
		goto done;

	hr = pHandler->SetCurrentMediaType(pType);

done:
	SafeRelease(&pPD);
	SafeRelease(&pSD);
	SafeRelease(&pHandler);
	SafeRelease(&pType);
	return hr;
}

HRESULT LogAttributeValueByIndex(IMFAttributes *pAttr, DWORD index)
{
	WCHAR *pGuidName = NULL;
	WCHAR *pGuidValName = NULL;

	GUID guid = { 0 };

	PROPVARIANT var;
	PropVariantInit(&var);

	HRESULT hr = pAttr->GetItemByIndex(index, &guid, &var);
	if (FAILED(hr))
		goto done;

	hr = GetGUIDName(guid, &pGuidName);
	if (FAILED(hr))
		goto done;

	DBGMSG(L"\t%s\t", pGuidName);

	hr = SpecialCaseAttributeValue(guid, var);
	if (FAILED(hr))
		goto done;
	if (hr == S_FALSE)
	{
		switch (var.vt)
		{
		case VT_UI4:
			DBGMSG(L"%d", var.ulVal);
			break;

		case VT_UI8:
			DBGMSG(L"%I64d", var.uhVal);
			break;

		case VT_R8:
			DBGMSG(L"%f", var.dblVal);
			break;

		case VT_CLSID:
			hr = GetGUIDName(*var.puuid, &pGuidValName);
			if (SUCCEEDED(hr))
				DBGMSG(pGuidValName);
			break;

		case VT_LPWSTR:
			DBGMSG(var.pwszVal);
			break;

		case VT_VECTOR | VT_UI1:
			DBGMSG(L"<<byte array>>");
			break;

		case VT_UNKNOWN:
			DBGMSG(L"IUnknown");
			break;

		default:
			DBGMSG(L"Unexpected attribute type (vt = %d)", var.vt);
			break;
		}
	}

done:
	DBGMSG(L"\n");
	CoTaskMemFree(pGuidName);
	CoTaskMemFree(pGuidValName);
	PropVariantClear(&var);
	return hr;
}


HRESULT GetGUIDName(const GUID& guid, WCHAR **ppwsz)
{
	HRESULT hr = S_OK;
	WCHAR *pName = NULL;

	LPCWSTR pcwsz = GetGUIDNameConst(guid);
	if (pcwsz)
	{
		size_t cchLength = 0;

		hr = StringCchLength(pcwsz, STRSAFE_MAX_CCH, &cchLength);
		if (FAILED(hr))
			goto done;

		pName = (WCHAR*)CoTaskMemAlloc((cchLength + 1) * sizeof(WCHAR));

		if (pName == NULL)
		{
			hr = E_OUTOFMEMORY;
			goto done;
		}

		hr = StringCchCopy(pName, cchLength + 1, pcwsz);
		if (FAILED(hr))
			goto done;
	}
	else
		hr = StringFromCLSID(guid, &pName);

done:
	if (FAILED(hr))
	{
		*ppwsz = NULL;
		CoTaskMemFree(pName);
	}
	else
		*ppwsz = pName;
	return hr;
}

void LogUINT32AsUINT64(const PROPVARIANT& var)
{
	UINT32 uHigh = 0, uLow = 0;
	Unpack2UINT32AsUINT64(var.uhVal.QuadPart, &uHigh, &uLow);
	DBGMSG(L"%d x %d", uHigh, uLow);
}

float OffsetToFloat(const MFOffset& offset)
{
	return offset.value + (static_cast<float>(offset.fract) / 65536.0f);
}

HRESULT LogVideoArea(const PROPVARIANT& var)
{
	if (var.caub.cElems < sizeof(MFVideoArea))
		return MF_E_BUFFERTOOSMALL;

	MFVideoArea *pArea = (MFVideoArea*)var.caub.pElems;

	DBGMSG(L"(%f,%f) (%d,%d)", OffsetToFloat(pArea->OffsetX), OffsetToFloat(pArea->OffsetY),
		pArea->Area.cx, pArea->Area.cy);
	return S_OK;
}

// Handle certain known special cases.
HRESULT SpecialCaseAttributeValue(GUID guid, const PROPVARIANT& var)
{
	if ((guid == MF_MT_FRAME_RATE) || (guid == MF_MT_FRAME_RATE_RANGE_MAX) ||
		(guid == MF_MT_FRAME_RATE_RANGE_MIN) || (guid == MF_MT_FRAME_SIZE) ||
		(guid == MF_MT_PIXEL_ASPECT_RATIO))
	{
		// Attributes that contain two packed 32-bit values.
		LogUINT32AsUINT64(var);
	}
	else if ((guid == MF_MT_GEOMETRIC_APERTURE) || (guid == MF_MT_MINIMUM_DISPLAY_APERTURE) ||
		(guid == MF_MT_PAN_SCAN_APERTURE))
		return LogVideoArea(var);  // Attributes that an MFVideoArea structure.
	else
		return S_FALSE;
	return S_OK;
}

void DBGMSG(PCWSTR format, ...)
{
	va_list args;
	va_start(args, format);

	WCHAR msg[MAX_PATH];

	if (SUCCEEDED(StringCbVPrintf(msg, sizeof(msg), format, args)))
		OutputDebugString(msg);
}

#ifndef IF_EQUAL_RETURN
#define IF_EQUAL_RETURN(param, val) if(val == param) return L#val
#endif

LPCWSTR GetGUIDNameConst(const GUID& guid)
{
	IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_SUBTYPE);
	IF_EQUAL_RETURN(guid, MF_MT_ALL_SAMPLES_INDEPENDENT);
	IF_EQUAL_RETURN(guid, MF_MT_FIXED_SIZE_SAMPLES);
	IF_EQUAL_RETURN(guid, MF_MT_COMPRESSED);
	IF_EQUAL_RETURN(guid, MF_MT_SAMPLE_SIZE);
	IF_EQUAL_RETURN(guid, MF_MT_WRAPPED_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_NUM_CHANNELS);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BLOCK_ALIGNMENT);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BITS_PER_SAMPLE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_VALID_BITS_PER_SAMPLE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_BLOCK);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_CHANNEL_MASK);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FOLDDOWN_MATRIX);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKREF);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKTARGET);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGREF);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGTARGET);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_PREFER_WAVEFORMATEX);
	IF_EQUAL_RETURN(guid, MF_MT_AAC_PAYLOAD_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_SIZE);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MAX);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MIN);
	IF_EQUAL_RETURN(guid, MF_MT_PIXEL_ASPECT_RATIO);
	IF_EQUAL_RETURN(guid, MF_MT_DRM_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_PAD_CONTROL_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_SOURCE_CONTENT_HINT);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_CHROMA_SITING);
	IF_EQUAL_RETURN(guid, MF_MT_INTERLACE_MODE);
	IF_EQUAL_RETURN(guid, MF_MT_TRANSFER_FUNCTION);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_PRIMARIES);
	IF_EQUAL_RETURN(guid, MF_MT_CUSTOM_VIDEO_PRIMARIES);
	IF_EQUAL_RETURN(guid, MF_MT_YUV_MATRIX);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_LIGHTING);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_NOMINAL_RANGE);
	IF_EQUAL_RETURN(guid, MF_MT_GEOMETRIC_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_MINIMUM_DISPLAY_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_ENABLED);
	IF_EQUAL_RETURN(guid, MF_MT_AVG_BITRATE);
	IF_EQUAL_RETURN(guid, MF_MT_AVG_BIT_ERROR_RATE);
	IF_EQUAL_RETURN(guid, MF_MT_MAX_KEYFRAME_SPACING);
	IF_EQUAL_RETURN(guid, MF_MT_DEFAULT_STRIDE);
	IF_EQUAL_RETURN(guid, MF_MT_PALETTE);
	IF_EQUAL_RETURN(guid, MF_MT_USER_DATA);
	IF_EQUAL_RETURN(guid, MF_MT_AM_FORMAT_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG_START_TIME_CODE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_PROFILE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_LEVEL);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG_SEQUENCE_HEADER);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_0);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_0);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_1);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_1);
	IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_SRC_PACK);
	IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_CTRL_PACK);
	IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_HEADER);
	IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_FORMAT);
	IF_EQUAL_RETURN(guid, MF_MT_IMAGE_LOSS_TOLERANT);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG4_SAMPLE_DESCRIPTION);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY);
	IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_4CC);
	IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_WAVE_FORMAT_TAG);

	// Media types
	IF_EQUAL_RETURN(guid, MFMediaType_Audio);
	IF_EQUAL_RETURN(guid, MFMediaType_Video);
	IF_EQUAL_RETURN(guid, MFMediaType_Protected);
	IF_EQUAL_RETURN(guid, MFMediaType_SAMI);
	IF_EQUAL_RETURN(guid, MFMediaType_Script);
	IF_EQUAL_RETURN(guid, MFMediaType_Image);
	IF_EQUAL_RETURN(guid, MFMediaType_HTML);
	IF_EQUAL_RETURN(guid, MFMediaType_Binary);
	IF_EQUAL_RETURN(guid, MFMediaType_FileTransfer);

	IF_EQUAL_RETURN(guid, MFVideoFormat_AI44); //     FCC('AI44')
	IF_EQUAL_RETURN(guid, MFVideoFormat_ARGB32); //   D3DFMT_A8R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_AYUV); //     FCC('AYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV25); //     FCC('dv25')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV50); //     FCC('dv50')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVH1); //     FCC('dvh1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSD); //     FCC('dvsd')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSL); //     FCC('dvsl')
	IF_EQUAL_RETURN(guid, MFVideoFormat_H264); //     FCC('H264')
	IF_EQUAL_RETURN(guid, MFVideoFormat_I420); //     FCC('I420')
	IF_EQUAL_RETURN(guid, MFVideoFormat_IYUV); //     FCC('IYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_M4S2); //     FCC('M4S2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MJPG);
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP43); //     FCC('MP43')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4S); //     FCC('MP4S')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4V); //     FCC('MP4V')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MPG1); //     FCC('MPG1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS1); //     FCC('MSS1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS2); //     FCC('MSS2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV11); //     FCC('NV11')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV12); //     FCC('NV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P010); //     FCC('P010')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P016); //     FCC('P016')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P210); //     FCC('P210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P216); //     FCC('P216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB24); //    D3DFMT_R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB32); //    D3DFMT_X8R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB555); //   D3DFMT_X1R5G5B5 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB565); //   D3DFMT_R5G6B5 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB8);
	IF_EQUAL_RETURN(guid, MFVideoFormat_UYVY); //     FCC('UYVY')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v210); //     FCC('v210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v410); //     FCC('v410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV1); //     FCC('WMV1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV2); //     FCC('WMV2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV3); //     FCC('WMV3')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WVC1); //     FCC('WVC1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y210); //     FCC('Y210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y216); //     FCC('Y216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y410); //     FCC('Y410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y416); //     FCC('Y416')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41P);
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41T);
	IF_EQUAL_RETURN(guid, MFVideoFormat_YUY2); //     FCC('YUY2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YV12); //     FCC('YV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YVYU);

	IF_EQUAL_RETURN(guid, MFAudioFormat_PCM); //              WAVE_FORMAT_PCM 
	IF_EQUAL_RETURN(guid, MFAudioFormat_Float); //            WAVE_FORMAT_IEEE_FLOAT 
	IF_EQUAL_RETURN(guid, MFAudioFormat_DTS); //              WAVE_FORMAT_DTS 
	IF_EQUAL_RETURN(guid, MFAudioFormat_Dolby_AC3_SPDIF); //  WAVE_FORMAT_DOLBY_AC3_SPDIF 
	IF_EQUAL_RETURN(guid, MFAudioFormat_DRM); //              WAVE_FORMAT_DRM 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV8); //        WAVE_FORMAT_WMAUDIO2 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV9); //        WAVE_FORMAT_WMAUDIO3 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudio_Lossless); // WAVE_FORMAT_WMAUDIO_LOSSLESS 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMASPDIF); //         WAVE_FORMAT_WMASPDIF 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MSP1); //             WAVE_FORMAT_WMAVOICE9 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MP3); //              WAVE_FORMAT_MPEGLAYER3 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MPEG); //             WAVE_FORMAT_MPEG 
	IF_EQUAL_RETURN(guid, MFAudioFormat_AAC); //              WAVE_FORMAT_MPEG_HEAAC 
	IF_EQUAL_RETURN(guid, MFAudioFormat_ADTS); //             WAVE_FORMAT_MPEG_ADTS_AAC 

	return NULL;
}

void saveBMP(unsigned char* data, int num, int bmpWidth, int bmpHeight)
{
	char str[20];
	FILE* fp;
	sprintf(str, "bmpfile\\%d.bmp", num);

	if ((fp = fopen(str, "wb")) == NULL)
	{
		cout << "create the bmp file error!" << endl;
		return;
	}

	int bytesofScanLine, i, j;
	DWORD dwFileSize;

	bytesofScanLine = (bmpWidth % 4 == 0) ? bmpWidth : ((bmpWidth + 3) / 4 * 4);
	dwFileSize = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)* 256 + bytesofScanLine*bmpHeight;

	BITMAPFILEHEADER bmfHeader;
	bmfHeader.bfType = MAKEWORD('B', 'M');
	bmfHeader.bfSize = dwFileSize;
	bmfHeader.bfReserved1 = 0;
	bmfHeader.bfReserved2 = 0;
	bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)* 256;
	fwrite(&bmfHeader, sizeof(bmfHeader), 1, fp);

	// fill the bmp file Infomation Header.
	BITMAPINFOHEADER bmiHeader;
	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = bmpWidth;
	bmiHeader.biHeight = bmpHeight;
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 8;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0;
	bmiHeader.biXPelsPerMeter = 0;
	bmiHeader.biYPelsPerMeter = 0;
	bmiHeader.biClrUsed = 256;
	bmiHeader.biClrImportant = 256;
	fwrite(&bmiHeader, sizeof(bmiHeader), 1, fp);

	RGBQUAD bmiColors[256];
	for (i = 0; i < 256; i++)  // fill the color tables.
	{
		bmiColors[i].rgbBlue = (BYTE)(i);
		bmiColors[i].rgbGreen = (BYTE)(i);
		bmiColors[i].rgbRed = (BYTE)(i);
		bmiColors[i].rgbReserved = 0;
	}
	fwrite(&bmiColors[0], sizeof(RGBQUAD)* 256, 1, fp);

	for (i = 0; i < bmpHeight; ++i)  // fill the BMP data to file
		for (j = 0; j < bmpWidth; ++j)
			fwrite(&((*(data + (bmpHeight - i - 1)* bmpWidth + j))), 1, sizeof(BYTE), fp);

	fclose(fp);
}
