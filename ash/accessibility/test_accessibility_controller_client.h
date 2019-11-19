// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_
#define ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_

#include "ash/public/cpp/accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/macros.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ash {

// Implement AccessibilityControllerClient mojo interface to simulate chrome
// behavior in tests. This breaks the ash/chrome dependency to allow testing ash
// code in isolation.
class TestAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  TestAccessibilityControllerClient();
  ~TestAccessibilityControllerClient();

  static constexpr base::TimeDelta kShutdownSoundDuration =
      base::TimeDelta::FromMilliseconds(1000);

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(AccessibilityAlert alert) override;
  void TriggerAccessibilityAlertWithMessage(
      const std::string& message) override;
  void PlayEarcon(int32_t sound_key) override;
  base::TimeDelta PlayShutdownSound() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override;
  bool ToggleDictation() override;
  void SilenceSpokenFeedback() override;
  void OnTwoFingerTouchStart() override;
  void OnTwoFingerTouchStop() override;
  bool ShouldToggleSpokenFeedbackViaTouch() const override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void RequestSelectToSpeakStateChange() override;
  void RequestAutoclickScrollableBoundsForPoint(
      gfx::Point& point_in_screen) override;

  int32_t GetPlayedEarconAndReset();

  AccessibilityAlert last_a11y_alert() const { return last_a11y_alert_; }
  ax::mojom::Gesture last_a11y_gesture() const { return last_a11y_gesture_; }
  int select_to_speak_change_change_requests() const {
    return select_to_speak_state_change_requests_;
  }

 private:
  AccessibilityAlert last_a11y_alert_ = AccessibilityAlert::NONE;

  int32_t sound_key_ = -1;
  bool is_dictation_active_ = false;

  ax::mojom::Gesture last_a11y_gesture_ = ax::mojom::Gesture::kNone;

  int select_to_speak_state_change_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityControllerClient);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_
