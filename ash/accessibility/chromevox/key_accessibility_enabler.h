// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CHROMEVOX_KEY_ACCESSIBILITY_ENABLER_H_
#define ASH_ACCESSIBILITY_CHROMEVOX_KEY_ACCESSIBILITY_ENABLER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {
class SpokenFeedbackEnabler;

// Toggles spoken feedback when the volume up and volume down keys are held for
// at least 5 seconds.
class ASH_EXPORT KeyAccessibilityEnabler : public ui::EventHandler {
 public:
  KeyAccessibilityEnabler();

  KeyAccessibilityEnabler(const KeyAccessibilityEnabler&) = delete;
  KeyAccessibilityEnabler& operator=(const KeyAccessibilityEnabler&) = delete;

  ~KeyAccessibilityEnabler() override;

 private:
  friend class KeyAccessibilityEnablerTest;

  // Overridden from ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override;

  std::unique_ptr<SpokenFeedbackEnabler> spoken_feedback_enabler_;
  bool vol_down_pressed_ = false;
  bool vol_up_pressed_ = false;
  bool other_key_pressed_ = false;
  base::TimeTicks first_time_both_volume_keys_pressed_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CHROMEVOX_KEY_ACCESSIBILITY_ENABLER_H_
