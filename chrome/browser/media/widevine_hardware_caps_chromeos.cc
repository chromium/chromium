// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/widevine_hardware_caps.h"

#include "base/check.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"

void GetWidevineHardwareCaps(
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  // We currently support VP9, H264 and HEVC.
  video_codecs->insert(media::VideoCodec::kCodecVP9);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  video_codecs->insert(media::VideoCodec::kCodecH264);
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  video_codecs->insert(media::VideoCodec::kCodecHEVC);
#endif

  // Both encryption schemes are supported on ChromeOS.
  encryption_schemes->insert(media::EncryptionScheme::kCenc);
  encryption_schemes->insert(media::EncryptionScheme::kCbcs);
}
