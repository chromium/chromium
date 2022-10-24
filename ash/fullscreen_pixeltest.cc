// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"
#include "ash/test/ash_pixel_test_init_params.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class FullscreenPixelTest : public AshTestBase {
 public:
  FullscreenPixelTest() {
    PrepareForPixelDiffTest(/*screenshot_prefix=*/"fullscreen_test",
                            pixel_test::InitParams());
  }
  ~FullscreenPixelTest() override = default;

  bool ComparePrimaryFullScreen(const std::string& screenshot_name) {
    return GetPixelDiffer()->ComparePrimaryFullScreen(screenshot_name);
  }
};

// Verifies the primary fullscreen of an active user session.
TEST_F(FullscreenPixelTest, VerifyDefaultPrimaryDisplay) {
  EXPECT_TRUE(ComparePrimaryFullScreen("primary_display"));
}

}  // namespace ash