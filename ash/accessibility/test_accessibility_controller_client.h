// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLIENT_H_
#define ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLIENT_H_

#include <optional>

#include "ash/public/cpp/accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ash {

// Implement AccessibilityControllerClient mojo interface to simulate chrome
// behavior in tests. This breaks the ash/chrome dependency to allow testing ash
// code in isolation.
class TestAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  TestAccessibilityControllerClient();

  TestAccessibilityControllerClient(const TestAccessibilityControllerClient&) =
      delete;
  TestAccessibilityControllerClient& operator=(
      const TestAccessibilityControllerClient&) = delete;

  ~TestAccessibilityControllerClient();

  static constexpr base::TimeDelta kShutdownSoundDuration =
      base::Milliseconds(1000);

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(AccessibilityAlert alert) override;
  void TriggerAccessibilityAlertWithMessage(
      const std::string& message) override;
  void PlayEarcon(Sound sound_key) override;
  base::TimeDelta PlayShutdownSound() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location) override;
  bool ToggleDictation() override;
  void SilenceSpokenFeedback() override;
  bool ShouldToggleSpokenFeedbackViaTouch() const override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void RequestSelectToSpeakStateChange() override;
  void RequestAutoclickScrollableBoundsForPoint(
      const gfx::Point& point_in_screen) override;
  void MagnifierBoundsChanged(const gfx::Rect& bounds_in_screen) override;
  void OnSwitchAccessDisabled() override;
  void OnSelectToSpeakPanelAction(SelectToSpeakPanelAction action,
                                  double value) override;
  void SetA11yOverrideWindow(aura::Window* a11y_override_window) override;
  std::string GetDictationDefaultLocale(bool new_user) override;

  std::optional<Sound> GetPlayedEarconAndReset();

  AccessibilityAlert last_a11y_alert() const { return last_a11y_alert_; }
  ax::mojom::Gesture last_a11y_gesture() const { return last_a11y_gesture_; }
  int select_to_speak_change_change_requests() const {
    return select_to_speak_state_change_requests_;
  }
  const std::string& last_alert_message() const { return last_alert_message_; }
  SelectToSpeakPanelAction last_select_to_speak_panel_action() const {
    return last_select_to_speak_panel_action_;
  }
  double last_select_to_speak_panel_action_value() const {
    return last_select_to_speak_panel_action_value_;
  }

 private:
  AccessibilityAlert last_a11y_alert_ = AccessibilityAlert::NONE;
  std::string last_alert_message_;
  std::optional<Sound> sound_key_;
  bool is_dictation_active_ = false;
  SelectToSpeakPanelAction last_select_to_speak_panel_action_ =
      SelectToSpeakPanelAction::kNone;
  double last_select_to_speak_panel_action_value_ = 0.0;

  ax::mojom::Gesture last_a11y_gesture_ = ax::mojom::Gesture::kNone;

  int select_to_speak_state_change_requests_ = 0;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLIENT_H_
