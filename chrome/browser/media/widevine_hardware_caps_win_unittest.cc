// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/widevine_hardware_caps.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(WidevineHardwareCapsTest, GetWidevineHardwareCaps) {
  base::flat_set<media::VideoCodec> video_codecs;
  base::flat_set<media::EncryptionScheme> encryption_schemes;

  // Not checking the results since it's hardware dependent.
  GetWidevineHardwareCaps(&video_codecs, &encryption_schemes);
}
