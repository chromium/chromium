// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/voice_interaction/voice_interaction_controller.h"

#include <utility>

#include "chromeos/chromeos_switches.h"

namespace ash {

VoiceInteractionController::VoiceInteractionController() {
  if (chromeos::switches::IsAssistantEnabled())
    voice_interaction_state_ = mojom::VoiceInteractionState::NOT_READY;
}

VoiceInteractionController::~VoiceInteractionController() = default;

void VoiceInteractionController::BindRequest(
    mojom::VoiceInteractionControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VoiceInteractionController::NotifyStatusChanged(
    mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnVoiceInteractionStatusChanged(state);
  });
}

void VoiceInteractionController::NotifySettingsEnabled(bool enabled) {
  settings_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionSettingsEnabled(enabled);
  });
}

void VoiceInteractionController::NotifyContextEnabled(bool enabled) {
  context_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionContextEnabled(enabled);
  });
}

void VoiceInteractionController::NotifyHotwordEnabled(bool enabled) {
  hotword_enabled_ = enabled;
  observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnVoiceInteractionHotwordEnabled(enabled);
  });
}

void VoiceInteractionController::NotifySetupCompleted(bool completed) {
  setup_completed_ = completed;
  observers_.ForAllPtrs([completed](auto* observer) {
    observer->OnVoiceInteractionSetupCompleted(completed);
  });
}

void VoiceInteractionController::NotifyFeatureAllowed(
    mojom::AssistantAllowedState state) {
  allowed_state_ = state;
  observers_.ForAllPtrs([state](auto* observer) {
    observer->OnAssistantFeatureAllowedChanged(state);
  });
}

void VoiceInteractionController::NotifyNotificationEnabled(bool enabled) {
  notification_enabled_ = enabled;
}

void VoiceInteractionController::NotifyLocaleChanged(
    const std::string& locale) {
  locale_ = locale;
  observers_.ForAllPtrs(
      [locale](auto* observer) { observer->OnLocaleChanged(locale); });
}

void VoiceInteractionController::NotifyLaunchWithMicOpen(
    bool launch_with_mic_open) {
  launch_with_mic_open_ = launch_with_mic_open;
}

void VoiceInteractionController::IsSettingEnabled(
    IsSettingEnabledCallback callback) {
  std::move(callback).Run(settings_enabled_);
}

void VoiceInteractionController::IsSetupCompleted(
    IsSetupCompletedCallback callback) {
  std::move(callback).Run(setup_completed_);
}

void VoiceInteractionController::IsContextEnabled(
    IsContextEnabledCallback callback) {
  std::move(callback).Run(context_enabled_);
}

void VoiceInteractionController::IsHotwordEnabled(
    IsHotwordEnabledCallback callback) {
  std::move(callback).Run(hotword_enabled_);
}

void VoiceInteractionController::AddObserver(
    mojom::VoiceInteractionObserverPtr observer) {
  // Locale needs to be notified when adding observer as this property is
  // changed at profile initialization and some observers (e.g. assistant) may
  // be constructed at later timing.
  observer->OnLocaleChanged(locale_);
  observers_.AddPtr(std::move(observer));
}

void VoiceInteractionController::FlushForTesting() {
  observers_.FlushForTesting();
}

}  // namespace ash
