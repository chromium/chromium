// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/key_accessibility_enabler.h"

#include "ash/accessibility/spoken_feedback_enabler.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace ash {

KeyAccessibilityEnabler::KeyAccessibilityEnabler() {
  Shell::Get()->AddPreTargetHandler(this,
                                    ui::EventTarget::Priority::kAccessibility);
}

KeyAccessibilityEnabler::~KeyAccessibilityEnabler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void KeyAccessibilityEnabler::OnKeyEvent(ui::KeyEvent* event) {
  if ((event->type() != ui::ET_KEY_PRESSED &&
       event->type() != ui::ET_KEY_RELEASED) ||
      !Shell::Get()->tablet_mode_controller()->InTabletMode())
    return;

  if (event->key_code() == ui::VKEY_VOLUME_DOWN)
    vol_down_pressed_ = event->type() == ui::ET_KEY_PRESSED;
  else if (event->key_code() == ui::VKEY_VOLUME_UP)
    vol_up_pressed_ = event->type() == ui::ET_KEY_PRESSED;
  else
    other_key_pressed_ = event->type() == ui::ET_KEY_PRESSED;

  if (vol_down_pressed_ && vol_up_pressed_ && !other_key_pressed_) {
    if (!spoken_feedback_enabler_.get()) {
      spoken_feedback_enabler_ = std::make_unique<SpokenFeedbackEnabler>();
      first_time_both_volume_keys_pressed_ = ui::EventTimeForNow();
    }

    if (ui::EventTimeForNow() - first_time_both_volume_keys_pressed_ >
        PowerButtonScreenshotController::kScreenshotChordDelay)
      event->StopPropagation();
  } else if (spoken_feedback_enabler_.get()) {
    spoken_feedback_enabler_.reset();
  }
}

}  // namespace ash
