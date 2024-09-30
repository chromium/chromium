// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_caps_lock_state_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using PickerCapsLockStateViewTest = views::ViewsTestBase;

TEST_F(PickerCapsLockStateViewTest, ShowsCapsLockOn) {
  PickerCapsLockStateView* view =
      new PickerCapsLockStateView(GetContext(), true, gfx::Rect(0, 0, 120, 20));

  EXPECT_EQ(view->label_for_testing()->GetText(), u"Caps Lock on");
}

TEST_F(PickerCapsLockStateViewTest, ShowsCapsLockOff) {
  PickerCapsLockStateView* view = new PickerCapsLockStateView(
      GetContext(), false, gfx::Rect(0, 0, 120, 20));

  EXPECT_EQ(view->label_for_testing()->GetText(), u"Caps Lock off");
}

}  // namespace
}  // namespace ash
