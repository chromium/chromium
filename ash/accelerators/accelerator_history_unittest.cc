// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_history_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

TEST(AcceleratorHistoryImplTest, SimulatePressAndHold) {
  AcceleratorHistoryImpl history;
  ui::Accelerator alt_press(ui::VKEY_MENU, ui::EF_NONE,
                            ui::Accelerator::KeyState::PRESSED);
  history.StoreCurrentAccelerator(alt_press);
  EXPECT_EQ(alt_press, history.current_accelerator());

  // Repeats don't affect previous accelerators.
  history.StoreCurrentAccelerator(alt_press);
  EXPECT_EQ(alt_press, history.current_accelerator());
  EXPECT_NE(alt_press, history.previous_accelerator());

  ui::Accelerator search_alt_press(ui::VKEY_LWIN, ui::EF_ALT_DOWN,
                                   ui::Accelerator::KeyState::PRESSED);
  history.StoreCurrentAccelerator(search_alt_press);
  EXPECT_EQ(search_alt_press, history.current_accelerator());
  EXPECT_EQ(alt_press, history.previous_accelerator());
  history.StoreCurrentAccelerator(search_alt_press);
  EXPECT_EQ(search_alt_press, history.current_accelerator());
  EXPECT_EQ(alt_press, history.previous_accelerator());

  ui::Accelerator alt_release_search_down(ui::VKEY_MENU, ui::EF_COMMAND_DOWN,
                                          ui::Accelerator::KeyState::RELEASED);
  history.StoreCurrentAccelerator(alt_release_search_down);
  EXPECT_EQ(alt_release_search_down, history.current_accelerator());
  EXPECT_EQ(search_alt_press, history.previous_accelerator());

  // Search is still down and search presses will keep being generated, but from
  // the perspective of the AcceleratorHistoryImpl, this is the same Search
  // press that hasn't been released yet.
  ui::Accelerator search_press(ui::VKEY_LWIN, ui::EF_NONE,
                               ui::Accelerator::KeyState::PRESSED);
  history.StoreCurrentAccelerator(search_press);
  history.StoreCurrentAccelerator(search_press);
  history.StoreCurrentAccelerator(search_press);
  EXPECT_EQ(alt_release_search_down, history.current_accelerator());
  EXPECT_EQ(search_alt_press, history.previous_accelerator());

  ui::Accelerator search_release(ui::VKEY_LWIN, ui::EF_NONE,
                                 ui::Accelerator::KeyState::RELEASED);
  history.StoreCurrentAccelerator(search_release);
  EXPECT_EQ(search_release, history.current_accelerator());
  EXPECT_EQ(alt_release_search_down, history.previous_accelerator());
}

// Tests that the record of pressed keys is cleared when language changes
// between key press and release. Detected via a release event that arrives with
// no corresponding press. See https://crbug.com/1184474.
TEST(AcceleratorHistoryImplTest, ReleaseWithNoMatchingPressClearsPressedKeys) {
  // Press "]" aka ui::VKEY_OEM_6 in the US keyboard.
  AcceleratorHistoryImpl history;
  ui::Accelerator right_bracket_press(ui::VKEY_OEM_6, ui::EF_NONE,
                                      ui::Accelerator::KeyState::PRESSED);
  history.StoreCurrentAccelerator(right_bracket_press);

  // Simulate that a keyboard language change turns the release of "]" to
  // VKEY_OEM_PLUS in a DE keyboard. So there should be a release event with no
  // matching press. Test that the set of pressed keys is empty.
  ui::Accelerator right_bracket_release(ui::VKEY_OEM_PLUS, ui::EF_NONE,
                                        ui::Accelerator::KeyState::RELEASED);
  history.StoreCurrentAccelerator(right_bracket_release);
  EXPECT_TRUE(history.currently_pressed_keys().empty());
}

}  // namespace ash
