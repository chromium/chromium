// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/widevine_hardware_caps_win.h"

// Need format off to keep the include order which is important.
// clang-format off
#include <comdef.h>
#include <initguid.h>
#include <d3d11_1.h>
#include <stdint.h>
#include <wrl/client.h>
#include <bitset>
// clang-format on

#include "base/macros.h"
#include "base/stl_util.h"
#include "media/base/decrypt_config.h"
#include "media/media_buildflags.h"

namespace {

// Alias for printing HRESULT.
const auto PrintHr = logging::SystemErrorCodeToString;

// TODO(xhwang): Deduplicate this file and widevine_cdm_proxy_factory.cc.
// clang-format off
DEFINE_GUID(kD3DCryptoTypeIntelWidevine,
  0x586e681, 0x4e14, 0x4133, 0x85, 0xe5, 0xa1, 0x4, 0x1f, 0x59, 0x9e, 0x26);
// clang-format on

// Bit indices for Intel Widevine hardware secure decryption capabilities.
//
// Encryption schemes are defined in ISO/IEC 23001-7, "Common encryption in ISO
// base media file format files". Version 1 refers to ISO/IEC 23001-7:2012.
// Version 3 refers to ISO/IEC 23001-7:2016. The difference that matters in this
// context is as follows:
// - In Version 1, section 9.5, "In full sample encryption, the entire sample is
//   encrypted".
// - In Version 3, section 9.4.1, "Full sample encryption MAY be used for all
//   encrypted media types other than NAL Structured video, which SHALL use
//   Subsample encryption."
// Therefore with kCencVersion1, it is possible that an entire sample of NAL
// Structured video is encrypted. This is not allowed with kCencVersion3.
enum IntelWidevineCaps {
  kSupported = 0,
  kAesCtr = 8,
  kAesCbc = 9,
  kCencVersion1 = 10,
  kCencVersion3 = 11,
  kCbcs = 17,
};

struct CodecToD3D11DecoderProfile {
  media::VideoCodec video_codec;
  GUID d3d11_decoder_profile;
};

const CodecToD3D11DecoderProfile kCodecsToQuery[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::VideoCodec::kCodecH264, D3D11_DECODER_PROFILE_H264_VLD_NOFGT},
#endif
    {media::VideoCodec::kCodecVP9, D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0},
};

// Use |video_device| to help check whether |d3d11_decoder_profile| is supported
// with CENC.
bool IsD3D11DecoderProfileSupportedWithCenc(ID3D11VideoDevice* video_device,
                                            const GUID& d3d11_decoder_profile) {
  D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps = {};

  // Check whether kD3DCryptoTypeIntelWidevine is supported with |codec|.
  auto hresult = video_device->GetContentProtectionCaps(
      &kD3DCryptoTypeIntelWidevine, &d3d11_decoder_profile, &caps);
  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to GetContentProtectionCaps: " << PrintHr(hresult);
    return false;
  }

  // For kD3DCryptoTypeIntelWidevine, this is a bitmask of IntelWidevineCaps.
  auto capability = std::bitset<64>(caps.ProtectedMemorySize);
  DVLOG(1) << "Content protection caps: " << capability;

  if (!capability.test(IntelWidevineCaps::kSupported)) {
    DVLOG(1) << "Hardware secure decryption not supported";
    return false;
  }

  if (!capability.test(IntelWidevineCaps::kAesCtr)) {
    DVLOG(1) << "AES-CTR decryption not supported";
    return false;
  }

  // Query for CENC.
  // TODO(crbug.com/899984): There are contents encrypted with kCencVersion1 out
  // there, so this check is not sufficient. Update this to check kCencVersion1.
  if (!capability.test(IntelWidevineCaps::kCencVersion3)) {
    DVLOG(1) << "CENC version 3 not supported";
    return false;
  }

  DVLOG(1) << "Widevine hardware secure CENC-v3 decryption supported. CENC-v1 "
              "playback may fail!";
  return true;
}

}  // namespace

void GetWidevineHardwareCaps(
    const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(!cdm_proxy_protocols.empty());
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  // We only support kD3DCryptoTypeIntelWidevine.
  if (!cdm_proxy_protocols.count(media::CdmProxy::Protocol::kIntel)) {
    DVLOG(1) << "CDM supported CdmProxy protocol not supported by the system";
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;

  // D3D11CdmProxy requires D3D_FEATURE_LEVEL_11_1.
  const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};

  // Create device and populate |device|.
  HRESULT hresult = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels,
      base::size(feature_levels), D3D11_SDK_VERSION, device.GetAddressOf(),
      nullptr, nullptr);

  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to create the D3D11Device: " << PrintHr(hresult);
    return;
  }

  hresult = device.CopyTo(video_device.GetAddressOf());
  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to get ID3D11VideoDevice: " << PrintHr(hresult);
    return;
  }

  // TODO(xhwang): Support query for CBCS. Maybe return all encryption schemes
  // supported by a codec.

  for (const auto& entry : kCodecsToQuery) {
    if (IsD3D11DecoderProfileSupportedWithCenc(video_device.Get(),
                                               entry.d3d11_decoder_profile)) {
      video_codecs->insert(entry.video_codec);
    }
  }

  if (!video_codecs->empty())
    encryption_schemes->insert(media::EncryptionScheme::kCenc);
}
