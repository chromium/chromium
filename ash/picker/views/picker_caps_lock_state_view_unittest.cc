// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_caps_lock_state_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
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

}  // namespace
}  // namespace ash
