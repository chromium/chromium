// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_caps_lock_bubble_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {
namespace {

class PickerCapsLockBubbleControllerTest : public AshTestBase {
 public:
  PickerCapsLockBubbleControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(PickerCapsLockBubbleControllerTest,
       ToggleCapsLockWhenNotFocusedDoesNotShowBubble) {
  input_method::FakeImeKeyboard ime_keyboard;
  PickerCapsLockBubbleController controller(&ime_keyboard);

  ime_keyboard.SetCapsLockEnabled(true);

  EXPECT_FALSE(controller.bubble_view_for_testing());
}

TEST_F(PickerCapsLockBubbleControllerTest,
       ToggleCapsLockInTextFieldShowsBubbleForAShortTime) {
  input_method::FakeImeKeyboard ime_keyboard;
  PickerCapsLockBubbleController controller(&ime_keyboard);
  ui::FakeTextInputClient input_field(
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod(),
      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();

  ime_keyboard.SetCapsLockEnabled(true);

  EXPECT_TRUE(controller.bubble_view_for_testing());
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(controller.bubble_view_for_testing());
}

TEST_F(PickerCapsLockBubbleControllerTest,
       ToggleCapsLockTwiceQuicklyInTextFieldExtendsBubbleShowTime) {
  input_method::FakeImeKeyboard ime_keyboard;
  PickerCapsLockBubbleController controller(&ime_keyboard);
  ui::FakeTextInputClient input_field(
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod(),
      {.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.Focus();

  ime_keyboard.SetCapsLockEnabled(true);
  task_environment()->FastForwardBy(base::Seconds(2));
  ime_keyboard.SetCapsLockEnabled(false);

  EXPECT_TRUE(controller.bubble_view_for_testing());
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(controller.bubble_view_for_testing());
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(controller.bubble_view_for_testing());
}

}  // namespace
}  // namespace ash
