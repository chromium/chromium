// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/scrollable_shelf_test_base.h"
#include "ash/test/ash_pixel_diff_test_helper.h"
#include "ash/test/ash_pixel_test_init_params.h"

namespace ash {

class ScrollableShelfViewPixelRTLTest
    : public ScrollableShelfTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  ScrollableShelfViewPixelRTLTest() {
    PrepareForPixelDiffTest();
    if (GetParam()) {
      pixel_test::InitParams init_params;
      init_params.under_rtl = true;
      SetPixelTestInitParam(init_params);
    }
  }
  ScrollableShelfViewPixelRTLTest(const ScrollableShelfViewPixelRTLTest&) =
      delete;
  ScrollableShelfViewPixelRTLTest& operator=(
      const ScrollableShelfViewPixelRTLTest&) = delete;
  ~ScrollableShelfViewPixelRTLTest() override = default;

  // ScrollableShelfTestBase:
  void SetUp() override {
    ScrollableShelfTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff("scrollable_shelf_view_pixel");
    AddAppShortcutsUntilOverflow(/*use_alternative_color=*/true);
  }

  AshPixelDiffTestHelper pixel_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(RTL, ScrollableShelfViewPixelRTLTest, testing::Bool());

// Verifies the scrollable shelf under overflow.
TEST_P(ScrollableShelfViewPixelRTLTest, Basics) {
  EXPECT_TRUE(pixel_test_helper_.CompareUiComponentScreenshot(
      GetParam() ? "overflow_rtl" : "overflow",
      AshPixelDiffTestHelper::UiComponent::kShelfWidget));
}

}  // namespace ash
