// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/key_accessibility_enabler.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace ash {

class KeyAccessibilityEnablerTest : public AshTestBase,
                                    public AccessibilityObserver {
 public:
  KeyAccessibilityEnablerTest() {}

  void SetUp() override {
    ui::SetEventTickClockForTesting(&clock_);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->AddObserver(this);
    key_accessibility_enabler_ = Shell::Get()->key_accessibility_enabler();
  }

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  void SendKeyEvent(ui::KeyEvent* event) {
    // Tablet mode gets exited elsewhere, so we must force it enabled before
    // each key event.
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    key_accessibility_enabler_->OnKeyEvent(event);
  }

  void WaitForAccessibilityStatusChanged() {
    run_loop_ = std::make_unique<base::RunLoop>();
    clock_.Advance(base::TimeDelta::FromMilliseconds(5000));
    run_loop_->Run();
  }

 private:
  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override { run_loop_->Quit(); }

  std::unique_ptr<base::RunLoop> run_loop_;
  KeyAccessibilityEnabler* key_accessibility_enabler_;
  base::SimpleTestTickClock clock_;
};

TEST_F(KeyAccessibilityEnablerTest, TwoVolumeKeyDown) {
  ui::KeyEvent vol_down_press(ui::ET_KEY_PRESSED, ui::VKEY_VOLUME_DOWN,
                              ui::EF_NONE);
  ui::KeyEvent vol_up_press(ui::ET_KEY_PRESSED, ui::VKEY_VOLUME_UP,
                            ui::EF_NONE);
  ui::KeyEvent vol_down_release(ui::ET_KEY_RELEASED, ui::VKEY_VOLUME_DOWN,
                                ui::EF_NONE);
  ui::KeyEvent vol_up_release(ui::ET_KEY_RELEASED, ui::VKEY_VOLUME_UP,
                              ui::EF_NONE);

  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  ASSERT_FALSE(controller->spoken_feedback_enabled());
  SendKeyEvent(&vol_down_press);
  SendKeyEvent(&vol_up_press);
  WaitForAccessibilityStatusChanged();
  ASSERT_TRUE(controller->spoken_feedback_enabled());
  SendKeyEvent(&vol_down_release);
  SendKeyEvent(&vol_up_release);

  SendKeyEvent(&vol_down_press);
  SendKeyEvent(&vol_up_press);
  WaitForAccessibilityStatusChanged();
  ASSERT_FALSE(controller->spoken_feedback_enabled());
}

}  // namespace ash
