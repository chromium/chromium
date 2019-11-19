// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/test_accessibility_controller_client.h"

#include "ash/public/cpp/accessibility_controller.h"

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
    const std::string& message) {}

void TestAccessibilityControllerClient::PlayEarcon(int32_t sound_key) {
  sound_key_ = sound_key;
}

base::TimeDelta TestAccessibilityControllerClient::PlayShutdownSound() {
  return kShutdownSoundDuration;
}

void TestAccessibilityControllerClient::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  last_a11y_gesture_ = gesture;
}

bool TestAccessibilityControllerClient::ToggleDictation() {
  is_dictation_active_ = !is_dictation_active_;
  return is_dictation_active_;
}

void TestAccessibilityControllerClient::SilenceSpokenFeedback() {}

void TestAccessibilityControllerClient::OnTwoFingerTouchStart() {}

void TestAccessibilityControllerClient::OnTwoFingerTouchStop() {}

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
    RequestAutoclickScrollableBoundsForPoint(gfx::Point& point_in_screen) {}

int32_t TestAccessibilityControllerClient::GetPlayedEarconAndReset() {
  int32_t tmp = sound_key_;
  sound_key_ = -1;
  return tmp;
}

}  // namespace ash
