// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_caps_lock_state_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using QuickInsertCapsLockStateViewTest = views::ViewsTestBase;

TEST_F(QuickInsertCapsLockStateViewTest, ShowsCapsLockOn) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(0, 0, 120, 20));

  EXPECT_STREQ(view->icon_view_for_testing()
                   .GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               kQuickInsertCapsLockOnIcon.name);
}

TEST_F(QuickInsertCapsLockStateViewTest, ShowsCapsLockOff) {
  PickerCapsLockStateView* view = new PickerCapsLockStateView(
      GetContext(), false, gfx::Rect(0, 0, 120, 20));

  EXPECT_STREQ(view->icon_view_for_testing()
                   .GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               kQuickInsertCapsLockOffIcon.name);
}

class QuickInsertCapsLockStateViewRTLTest
    : public QuickInsertCapsLockStateViewTest,
      public testing::WithParamInterface<bool> {
 public:
  QuickInsertCapsLockStateViewRTLTest() {
    base::i18n::SetRTLForTesting(GetParam());
  }

 private:
  base::test::ScopedRestoreICUDefaultLocale restore_locale_;
};

INSTANTIATE_TEST_SUITE_P(,
                         QuickInsertCapsLockStateViewRTLTest,
                         testing::Values(true, false));

TEST_P(QuickInsertCapsLockStateViewRTLTest,
       ShowsCapsLockRightAlignedForLTRTextDirection) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(500, 0, 1, 1),
                                  base::i18n::TextDirection::LEFT_TO_RIGHT);
  view->Show();

  EXPECT_LT(view->GetBoundsInScreen().right(), 500);
}

TEST_P(QuickInsertCapsLockStateViewRTLTest,
       ShowsCapsLockLeftAlignedForRTLTextDirection) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(500, 0, 1, 1),
                                  base::i18n::TextDirection::RIGHT_TO_LEFT);
  view->Show();

  EXPECT_GT(view->GetBoundsInScreen().x(), 500);
}

TEST_P(QuickInsertCapsLockStateViewRTLTest,
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
