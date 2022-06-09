// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_base.h"

namespace ash {

class DemoAshPixelDiffTest : public AshPixelDiffTestBase {
 public:
  DemoAshPixelDiffTest() = default;
  DemoAshPixelDiffTest(const DemoAshPixelDiffTest&) = delete;
  DemoAshPixelDiffTest& operator=(const DemoAshPixelDiffTest&) = delete;
  ~DemoAshPixelDiffTest() override = default;

  // AshPixelDiffTestBase:
  void SetUp() override {
    AshPixelDiffTestBase::SetUp();
    pixel_diff()->Init(/*screenshot_prefix=*/"ash_demo_test");
  }
};

// Verifies the primary display UI right after the ash pixel test sets up.
TEST_F(DemoAshPixelDiffTest, VerifyDefaultPrimaryDisplay) {
  EXPECT_TRUE(ComparePrimaryFullScreen("primary_display"));
}

}  // namespace ash
