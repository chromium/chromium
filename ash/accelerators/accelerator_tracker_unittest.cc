// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_tracker.h"

#include "ash/test/ash_test_base.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/metrics/user_action_tester.h"

namespace ash {

using AcceleratorTrackerTest = AshTestBase;

constexpr base::StringPiece kUserActionPrefix = "AccelTracker_";

// Tests user action string starts with "AccelTracker_". The user action
// string locates in the kAcceleratorTrackerList table in accelerator_tracker.h
// file. Please make sure the user action strings in the table have this prefix.
TEST_F(AcceleratorTrackerTest, UserActionPrefix) {
  for (const auto& [_, user_action_name] : kAcceleratorTrackerList) {
    EXPECT_TRUE(base::StartsWith(user_action_name, kUserActionPrefix));
  }
}

// Tests AcceleratorTracker will only record key events that are in the
// initialized accelerator tracker map.
TEST_F(AcceleratorTrackerTest, TrackKeyEvent) {
  constexpr ui::KeyboardCode intended_key_code = ui::VKEY_A;
  constexpr ui::EventFlags intended_modifier =
      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;
  constexpr base::StringPiece intended_user_action =
      "AccelTracker_Ctrl_Shift_A";

  constexpr TrackerDataActionPair kAcceleratorTrackerListForTesting[] = {
      {{KeyState::PRESSED, intended_key_code, intended_modifier},
       intended_user_action},
  };

  AcceleratorTracker accelerator_tracker(kAcceleratorTrackerListForTesting);
  base::UserActionTester user_action_tester;

  // The metric is not recorded before the event is fired.
  EXPECT_EQ(0, user_action_tester.GetActionCount(intended_user_action.data()));

  // The metric is recorded after the event is fired.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, intended_key_code,
                         intended_modifier);
  accelerator_tracker.OnKeyEvent(&key_event);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));

  // Fire a similar key event won't trigger this metric.
  // An event with same key code and modifier, but different key states.
  ui::KeyEvent key_event1(ui::ET_KEY_RELEASED, intended_key_code,
                          intended_modifier);
  accelerator_tracker.OnKeyEvent(&key_event1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));

  // An event with same key code and key state, but superset of modifiers.
  ui::KeyEvent key_event2(ui::ET_KEY_PRESSED, intended_key_code,
                          intended_modifier | ui::EF_ALT_DOWN);
  accelerator_tracker.OnKeyEvent(&key_event2);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));

  // An event with same key code and key state, but subset of modifiers.
  ui::KeyEvent key_event3(ui::ET_KEY_PRESSED, intended_key_code,
                          ui::EF_CONTROL_DOWN);
  accelerator_tracker.OnKeyEvent(&key_event3);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));

  // An event with same key code and key state, but different modifiers.
  ui::KeyEvent key_event4(ui::ET_KEY_PRESSED, intended_key_code,
                          ui::EF_ALT_DOWN);
  accelerator_tracker.OnKeyEvent(&key_event4);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));

  // An event with same key state and modifiers, but different key codes.
  ui::KeyEvent key_event5(ui::ET_KEY_PRESSED, ui::VKEY_B, intended_modifier);
  accelerator_tracker.OnKeyEvent(&key_event5);
  EXPECT_EQ(1, user_action_tester.GetActionCount(intended_user_action.data()));
}

}  // namespace ash
