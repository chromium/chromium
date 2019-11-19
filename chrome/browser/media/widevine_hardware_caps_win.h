// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WIDEVINE_HARDWARE_CAPS_WIN_H_
#define CHROME_BROWSER_MEDIA_WIDEVINE_HARDWARE_CAPS_WIN_H_

#include "base/containers/flat_set.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_proxy.h"

namespace media {
enum class EncryptionScheme;
}

// Get supported Widevine hardware capabilities, including supported
// |video_codecs| and |encryption_schemes|.
void GetWidevineHardwareCaps(
    const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes);

#endif  // CHROME_BROWSER_MEDIA_WIDEVINE_HARDWARE_CAPS_WIN_H_
