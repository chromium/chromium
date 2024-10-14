// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_caps_lock_state_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using PickerCapsLockStateViewTest = views::ViewsTestBase;

TEST_F(PickerCapsLockStateViewTest, ShowsCapsLockOn) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(0, 0, 120, 20));

  EXPECT_STREQ(view->icon_view_for_testing()
                   .GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               kPickerCapsLockOnIcon.name);
}

TEST_F(PickerCapsLockStateViewTest, ShowsCapsLockOff) {
  PickerCapsLockStateView* view = new PickerCapsLockStateView(
      GetContext(), false, gfx::Rect(0, 0, 120, 20));

  EXPECT_STREQ(view->icon_view_for_testing()
                   .GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               kPickerCapsLockOffIcon.name);
}

class PickerCapsLockStateViewRTLTest
    : public PickerCapsLockStateViewTest,
      public testing::WithParamInterface<bool> {
 public:
  PickerCapsLockStateViewRTLTest() { base::i18n::SetRTLForTesting(GetParam()); }

 private:
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PickerCapsLockStateViewRTLTest,
                         testing::Values(true, false));

TEST_P(PickerCapsLockStateViewRTLTest,
       ShowsCapsLockRightAlignedForLTRTextDirection) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(500, 0, 1, 1),
                                  base::i18n::TextDirection::LEFT_TO_RIGHT);
  view->Show();

  EXPECT_LT(view->GetBoundsInScreen().right(), 500);
}

TEST_P(PickerCapsLockStateViewRTLTest,
       ShowsCapsLockLeftAlignedForRTLTextDirection) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(500, 0, 1, 1),
                                  base::i18n::TextDirection::RIGHT_TO_LEFT);
  view->Show();

  EXPECT_GT(view->GetBoundsInScreen().x(), 500);
}

TEST_P(PickerCapsLockStateViewRTLTest,
       ShowsCapsLockAlignedBasedOnLocaleForUnknownTextDirection) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(500, 0, 1, 1),
                                  base::i18n::TextDirection::UNKNOWN_DIRECTION);
  view->Show();

  if (GetParam()) {
    EXPECT_GT(view->GetBoundsInScreen().x(), 500);
  } else {
    EXPECT_LT(view->GetBoundsInScreen().right(), 500);
  }
}

}  // namespace
}  // namespace ash
