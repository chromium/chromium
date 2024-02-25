// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {

using SpokenFeedbackTogglerTest = AshTestBase;

TEST_F(SpokenFeedbackTogglerTest, Basic) {
  SpokenFeedbackToggler::ScopedEnablerForTest scoped;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  // Click and hold toggles the spoken feedback.
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // toggle again
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
}

TEST_F(SpokenFeedbackTogglerTest, PassThroughEvents) {
  SpokenFeedbackToggler::ScopedEnablerForTest scoped;

  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  window->Focus();

  ui::test::EventGenerator* generator = GetEventGenerator();

  // None hotkey events.
  generator->PressKey(ui::VKEY_A, 0);
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_EQ("1 1", delegate.GetKeyCountsAndReset());

  // Single hotkey press and release.
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_EQ("1 1", delegate.GetKeyCountsAndReset());

  // Hotkey press and hold.
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  generator->PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  generator->ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_EQ("3 1", delegate.GetKeyCountsAndReset());
}

}  // namespace ash
