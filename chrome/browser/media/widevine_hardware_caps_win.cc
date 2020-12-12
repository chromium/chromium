// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/widevine_hardware_caps.h"

#include "base/check.h"
#include "base/notreached.h"

void GetWidevineHardwareCaps(
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {
  DCHECK(video_codecs->empty());
  DCHECK(encryption_schemes->empty());

  NOTIMPLEMENTED();
}
