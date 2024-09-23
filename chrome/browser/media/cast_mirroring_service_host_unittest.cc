// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

TEST(CastMirroringServiceHostTest, TestGetClampedResolution) {
  // {screen width, screen height, capture width, capture height}
  constexpr int test_cases[][4] = {
      {1280, 800, 1280, 720},   {1366, 768, 1280, 720},
      {768, 1366, 1280, 720},   {1920, 1080, 1920, 1080},
      {1546, 2048, 1920, 1080}, {2399, 1598, 1920, 1080},
      {2560, 1080, 1920, 1080}, {2560, 1600, 1920, 1080},
      {3840, 2160, 1920, 1080}};

  for (int i = 0; i < 9; i++) {
    DVLOG(1) << "Testing resolution " << test_cases[i][0] << " x "
             << test_cases[i][1];
    EXPECT_EQ(CastMirroringServiceHost::GetClampedResolution(
                  gfx::Size(test_cases[i][0], test_cases[i][1])),
              gfx::Size(test_cases[i][2], test_cases[i][3]));
  }
}

}  // namespace mirroring
