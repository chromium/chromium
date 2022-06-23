// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class DemoAshPixelDiffTest : public AshTestBase {
 public:
  DemoAshPixelDiffTest() { PrepareForPixelDiffTest(); }
  DemoAshPixelDiffTest(const DemoAshPixelDiffTest&) = delete;
  DemoAshPixelDiffTest& operator=(const DemoAshPixelDiffTest&) = delete;
  ~DemoAshPixelDiffTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff(
        /*screenshot_prefix=*/"ash_demo_test");
  }

  AshPixelDiffTestHelper pixel_test_helper_;
};

// Verifies the primary display UI right after the ash pixel test sets up.
TEST_F(DemoAshPixelDiffTest, VerifyDefaultPrimaryDisplay) {
  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen("primary_display"));
}

}  // namespace ash
