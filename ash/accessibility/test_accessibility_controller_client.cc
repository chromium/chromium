// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/test_accessibility_controller_client.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

constexpr base::TimeDelta
    TestAccessibilityControllerClient::kShutdownSoundDuration;

TestAccessibilityControllerClient::TestAccessibilityControllerClient() {
  AccessibilityController::Get()->SetClient(this);
}

TestAccessibilityControllerClient::~TestAccessibilityControllerClient() {
  AccessibilityController::Get()->SetClient(nullptr);
}

void TestAccessibilityControllerClient::TriggerAccessibilityAlert(
    AccessibilityAlert alert) {
  last_a11y_alert_ = alert;
}

void TestAccessibilityControllerClient::TriggerAccessibilityAlertWithMessage(
    const std::string& message) {
  last_alert_message_ = message;
}

void TestAccessibilityControllerClient::PlayEarcon(Sound sound_key) {
  sound_key_ = sound_key;
}

base::TimeDelta TestAccessibilityControllerClient::PlayShutdownSound() {
  return kShutdownSoundDuration;
}

void TestAccessibilityControllerClient::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  last_a11y_gesture_ = gesture;
}

bool TestAccessibilityControllerClient::ToggleDictation() {
  is_dictation_active_ = !is_dictation_active_;
  return is_dictation_active_;
}

void TestAccessibilityControllerClient::SilenceSpokenFeedback() {}

bool TestAccessibilityControllerClient::ShouldToggleSpokenFeedbackViaTouch()
    const {
  return true;
}

void TestAccessibilityControllerClient::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {}

void TestAccessibilityControllerClient::RequestSelectToSpeakStateChange() {
  ++select_to_speak_state_change_requests_;
}

void TestAccessibilityControllerClient::
    RequestAutoclickScrollableBoundsForPoint(
        const gfx::Point& point_in_screen) {}

void TestAccessibilityControllerClient::MagnifierBoundsChanged(
    const gfx::Rect& bounds_in_screen) {}

void TestAccessibilityControllerClient::OnSwitchAccessDisabled() {}

void TestAccessibilityControllerClient::OnSelectToSpeakPanelAction(
    SelectToSpeakPanelAction action,
    double value) {
  last_select_to_speak_panel_action_ = action;
  last_select_to_speak_panel_action_value_ = value;
}

void TestAccessibilityControllerClient::SetA11yOverrideWindow(
    aura::Window* a11y_override_window) {}

std::string TestAccessibilityControllerClient::GetDictationDefaultLocale(
    bool new_user) {
  return "";
}

std::optional<Sound>
TestAccessibilityControllerClient::GetPlayedEarconAndReset() {
  return std::exchange(sound_key_, std::nullopt);
}

}  // namespace ash
