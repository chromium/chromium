// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/widevine_hardware_caps_win.h"

#include "testing/gtest/include/gtest/gtest.h"

// TODO(xhwang): Add tests using D3D11 mocks. Currently this cannot be done
// because we cannot depend on media/gpu/windows/d3d11_mocks.*.

TEST(WidevineHardwareCapsTest, GetWidevineHardwareCaps) {
  base::flat_set<media::CdmProxy::Protocol> cdm_proxy_protocols = {
      media::CdmProxy::Protocol::kIntel};
  base::flat_set<media::VideoCodec> video_codecs;
  base::flat_set<media::EncryptionScheme> encryption_schemes;

  // Not checking the results since it's hardware dependent.
  GetWidevineHardwareCaps(cdm_proxy_protocols, &video_codecs,
                          &encryption_schemes);
}
