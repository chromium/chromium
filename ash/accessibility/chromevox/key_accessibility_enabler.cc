// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/key_accessibility_enabler.h"

#include "ash/accessibility/chromevox/spoken_feedback_enabler.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace ash {

KeyAccessibilityEnabler::KeyAccessibilityEnabler() {
  Shell::Get()->AddAccessibilityEventHandler(
      this, AccessibilityEventHandlerManager::HandlerType::kChromeVox);
}

KeyAccessibilityEnabler::~KeyAccessibilityEnabler() {
  Shell::Get()->RemoveAccessibilityEventHandler(this);
}

void KeyAccessibilityEnabler::OnKeyEvent(ui::KeyEvent* event) {
  if ((event->type() != ui::EventType::kKeyPressed &&
       event->type() != ui::EventType::kKeyReleased) ||
      !display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  if (event->key_code() == ui::VKEY_VOLUME_DOWN) {
    vol_down_pressed_ = event->type() == ui::EventType::kKeyPressed;
  } else if (event->key_code() == ui::VKEY_VOLUME_UP) {
    vol_up_pressed_ = event->type() == ui::EventType::kKeyPressed;
  } else {
    other_key_pressed_ = event->type() == ui::EventType::kKeyPressed;
  }

  if (vol_down_pressed_ && vol_up_pressed_ && !other_key_pressed_) {
    if (!spoken_feedback_enabler_.get()) {
      spoken_feedback_enabler_ = std::make_unique<SpokenFeedbackEnabler>();
      first_time_both_volume_keys_pressed_ = ui::EventTimeForNow();
    }

    if (ui::EventTimeForNow() - first_time_both_volume_keys_pressed_ >
        PowerButtonScreenshotController::kScreenshotChordDelay) {
      event->StopPropagation();
    }
  } else if (spoken_feedback_enabler_.get()) {
    spoken_feedback_enabler_.reset();
  }
}

}  // namespace ash
