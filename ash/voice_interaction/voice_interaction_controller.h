// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_
#define ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ASH_EXPORT VoiceInteractionController
    : public mojom::VoiceInteractionController {
 public:
  VoiceInteractionController();
  ~VoiceInteractionController() override;

  void BindRequest(mojom::VoiceInteractionControllerRequest request);

  // ash::mojom::VoiceInteractionController:
  void NotifyStatusChanged(mojom::VoiceInteractionState state) override;
  void NotifySettingsEnabled(bool enabled) override;
  void NotifyContextEnabled(bool enabled) override;
  void NotifyHotwordEnabled(bool enabled) override;
  void NotifySetupCompleted(bool completed) override;
  void NotifyFeatureAllowed(mojom::AssistantAllowedState state) override;
  void NotifyNotificationEnabled(bool enabled) override;
  void NotifyLocaleChanged(const std::string& locale) override;
  void NotifyLaunchWithMicOpen(bool launch_with_mic_open) override;
  void IsSettingEnabled(IsSettingEnabledCallback callback) override;
  void IsSetupCompleted(IsSetupCompletedCallback callback) override;
  void IsContextEnabled(IsContextEnabledCallback callback) override;
  void IsHotwordEnabled(IsHotwordEnabledCallback callback) override;
  void AddObserver(mojom::VoiceInteractionObserverPtr observer) override;

  mojom::VoiceInteractionState voice_interaction_state() const {
    return voice_interaction_state_;
  }

  bool settings_enabled() const { return settings_enabled_; }

  bool setup_completed() const { return setup_completed_; }

  bool context_enabled() const { return context_enabled_; }

  mojom::AssistantAllowedState allowed_state() const { return allowed_state_; }

  bool notification_enabled() const { return notification_enabled_; }

  bool launch_with_mic_open() const { return launch_with_mic_open_; }

  void FlushForTesting();

 private:
  // Voice interaction state. The initial value should be set to STOPPED to make
  // sure the app list button burst animation could be correctly shown.
  mojom::VoiceInteractionState voice_interaction_state_ =
      mojom::VoiceInteractionState::STOPPED;

  // Whether voice interaction is enabled in system settings.
  bool settings_enabled_ = false;

  // Whether voice interaction setup flow has completed.
  bool setup_completed_ = false;

  // Whether screen context is enabled.
  bool context_enabled_ = false;

  // Whether hotword listening is enabled.
  bool hotword_enabled_ = false;

  // Whether notification is enabled.
  bool notification_enabled_ = false;

  // Whether voice interaction feature is allowed or disallowed for what reason.
  mojom::AssistantAllowedState allowed_state_ =
      mojom::AssistantAllowedState::ALLOWED;

  std::string locale_;

  // Whether the Assistant should launch with mic open;
  bool launch_with_mic_open_ = false;

  mojo::BindingSet<mojom::VoiceInteractionController> bindings_;

  mojo::InterfacePtrSet<mojom::VoiceInteractionObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionController);
};

}  // namespace ash

#endif  // ASH_VOICE_INTERACTION_VOICE_INTERACTION_CONTROLLER_H_
