// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/magnifier_key_scroller.h"

#include <memory>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class KeyEventDelegate : public aura::test::TestWindowDelegate {
 public:
  KeyEventDelegate() = default;

  KeyEventDelegate(const KeyEventDelegate&) = delete;
  KeyEventDelegate& operator=(const KeyEventDelegate&) = delete;

  ~KeyEventDelegate() override = default;

  // ui::EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* event) override {
    key_event = std::make_unique<ui::KeyEvent>(event->type(), event->key_code(),
                                               event->flags());
  }

  const ui::KeyEvent* event() const { return key_event.get(); }
  void reset() { key_event.reset(); }

 private:
  std::unique_ptr<ui::KeyEvent> key_event;
};

}  // namespace

using MagnifierKeyScrollerTest = AshTestBase;

TEST_F(MagnifierKeyScrollerTest, Basic) {
  KeyEventDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(10, 10, 100, 100)));
  wm::ActivateWindow(window.get());

  MagnifierKeyScroller::ScopedEnablerForTest scoped;
  FullscreenMagnifierController* controller =
      Shell::Get()->fullscreen_magnifier_controller();
  controller->SetEnabled(true);

  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Click and Release generates the press event upon release.
  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator->ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyPressed, delegate.event()->type());
  delegate.reset();

  // Click and hold scrolls the magnifier screen.
  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,150", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  EXPECT_EQ("200,300", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  generator->ReleaseKey(ui::VKEY_DOWN, 0);
  EXPECT_EQ("200,300", controller->GetWindowPosition().ToString());
  EXPECT_FALSE(delegate.event());

  // Events are passed normally when the magnifier is off.
  controller->SetEnabled(false);

  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyPressed, delegate.event()->type());
  delegate.reset();

  generator->ReleaseKey(ui::VKEY_DOWN, 0);
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyReleased, delegate.event()->type());
  delegate.reset();

  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyPressed, delegate.event()->type());
  delegate.reset();

  generator->PressKey(ui::VKEY_DOWN, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyPressed, delegate.event()->type());
  delegate.reset();

  generator->ReleaseKey(ui::VKEY_DOWN, 0);
  ASSERT_TRUE(delegate.event());
  EXPECT_EQ(ui::EventType::kKeyReleased, delegate.event()->type());
  delegate.reset();
}

}  // namespace ash
