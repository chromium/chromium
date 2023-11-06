// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_accessibility_provider_proxy.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"

namespace ash::eche_app {

EcheAppAccessibilityProviderProxy::EcheAppAccessibilityProviderProxy()
    : was_accessibility_enabled_(IsAccessibilityEnabled()) {}
EcheAppAccessibilityProviderProxy::~EcheAppAccessibilityProviderProxy() =
    default;

void EcheAppAccessibilityProviderProxy::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
          AccessibilityNotificationType::kToggleFocusHighlight &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSelectToSpeak &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSpokenFeedback &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSwitchAccess &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleDockedMagnifier &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleScreenMagnifier) {
    return;
  }
  UpdateEnabledFeature();

  // Check if the state of accessibility has changed.
  bool is_accessibility_enabled = IsAccessibilityEnabled();
  if (!is_accessibility_enabled == was_accessibility_enabled_) {
    was_accessibility_enabled_ = is_accessibility_enabled;
    if (accessibility_state_changed_callback_.has_value()) {
      accessibility_state_changed_callback_->Run(is_accessibility_enabled);
    }
  }

  if (event_details.notification_type ==
      AccessibilityNotificationType::kToggleSpokenFeedback) {
    if (explore_by_touch_state_changed_callback_.has_value()) {
      explore_by_touch_state_changed_callback_->Run(event_details.enabled);
    }
  }
}

ax::android::mojom::AccessibilityFilterType
EcheAppAccessibilityProviderProxy::GetFilterType() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  const MagnificationManager* magnification_manager =
      MagnificationManager::Get();

  if (!accessibility_manager || !magnification_manager) {
    return ax::android::mojom::AccessibilityFilterType::OFF;
  }

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled() ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      magnification_manager->IsMagnifierEnabled() ||
      magnification_manager->IsDockedMagnifierEnabled()) {
    return ax::android::mojom::AccessibilityFilterType::ALL;
  }

  if (accessibility_manager->IsFocusHighlightEnabled()) {
    return ax::android::mojom::AccessibilityFilterType::FOCUS;
  }

  return ax::android::mojom::AccessibilityFilterType::OFF;
}

bool EcheAppAccessibilityProviderProxy::UseFullFocusMode() {
  return use_full_focus_mode_;
}

bool EcheAppAccessibilityProviderProxy::IsAccessibilityEnabled() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  if (accessibility_manager) {
    return accessibility_manager->IsFocusHighlightEnabled() ||
           accessibility_manager->IsSelectToSpeakEnabled() ||
           accessibility_manager->IsSpokenFeedbackEnabled() ||
           accessibility_manager->IsSwitchAccessEnabled();
  }

  const MagnificationManager* magnification_manager =
      MagnificationManager::Get();

  if (magnification_manager) {
    return magnification_manager->IsDockedMagnifierEnabled() ||
           magnification_manager->IsMagnifierEnabled();
  }

  return false;
}

void EcheAppAccessibilityProviderProxy::OnViewTracked() {
  UpdateEnabledFeature();
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &EcheAppAccessibilityProviderProxy::OnAccessibilityStatusChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Send initial states.
  if (accessibility_state_changed_callback_.has_value()) {
    accessibility_state_changed_callback_->Run(IsAccessibilityEnabled());
  }

  if (explore_by_touch_state_changed_callback_.has_value()) {
    explore_by_touch_state_changed_callback_->Run(
        accessibility_manager->IsSpokenFeedbackEnabled());
  }
}

void EcheAppAccessibilityProviderProxy::
    SetAccessibilityEnabledStateChangedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  accessibility_state_changed_callback_ = callback;
}

void EcheAppAccessibilityProviderProxy::
    SetExploreByTouchEnabledStateChangedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  explore_by_touch_state_changed_callback_ = callback;
}

void EcheAppAccessibilityProviderProxy::UpdateEnabledFeature() {
  const AccessibilityManager* accessibility_manager =
      AccessibilityManager::Get();
  if (accessibility_manager) {
    use_full_focus_mode_ = accessibility_manager->IsSwitchAccessEnabled() ||
                           accessibility_manager->IsSpokenFeedbackEnabled();
  }
}
}  // namespace ash::eche_app
