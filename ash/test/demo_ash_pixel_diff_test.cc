// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class DemoAshPixelDiffTest : public AshTestBase {
 public:
  DemoAshPixelDiffTest() {
    PrepareForPixelDiffTest(/*screenshot_prefix=*/"ash_demo_test",
                            pixel_test::InitParams());
  }
  DemoAshPixelDiffTest(const DemoAshPixelDiffTest&) = delete;
  DemoAshPixelDiffTest& operator=(const DemoAshPixelDiffTest&) = delete;
  ~DemoAshPixelDiffTest() override = default;

  bool ComparePrimaryFullScreen(const std::string& screenshot_name) {
    return GetPixelDiffer()->ComparePrimaryFullScreen(screenshot_name);
  }
};

// Verifies the primary display UI right after the ash pixel test sets up.
TEST_F(DemoAshPixelDiffTest, VerifyDefaultPrimaryDisplay) {
  EXPECT_TRUE(ComparePrimaryFullScreen("primary_display"));
}

}  // namespace ash
