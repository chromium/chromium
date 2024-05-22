// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state_base.h"

#include <ostream>
#include <sstream>

#include "ash/public/cpp/accelerators.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

using assistant::prefs::AssistantOnboardingMode;

AssistantStateBase::AssistantStateBase() = default;

AssistantStateBase::~AssistantStateBase() {
  for (auto& observer : observers_)
    observer.OnAssistantStateDestroyed();
}

std::string AssistantStateBase::ToString() const {
#define STRINGIFY(field) \
  #field, (field.has_value() ? base::ToString(field.value()) : "(no value)")
  return base::StrCat(
      {"AssistantStatus: ", base::ToString(assistant_status_),
       STRINGIFY(settings_enabled()), STRINGIFY(context_enabled()),
       STRINGIFY(hotword_enabled()), STRINGIFY(allowed_state()),
       STRINGIFY(locale()), STRINGIFY(arc_play_store_enabled()),
       STRINGIFY(locked_full_screen_enabled()), STRINGIFY(onboarding_mode())});
#undef STRINGIFY
}

void AssistantStateBase::AddObserver(AssistantStateObserver* observer) {
  observers_.AddObserver(observer);
  InitializeObserver(observer);
}

void AssistantStateBase::RemoveObserver(AssistantStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantStateBase::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service)
    return;

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantConsentStatus,
      base::BindRepeating(&AssistantStateBase::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantContextEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateContextEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateSettingsEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantHotwordAlwaysOn,
      base::BindRepeating(&AssistantStateBase::UpdateHotwordAlwaysOn,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantHotwordEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateHotwordEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantLaunchWithMicOpen,
      base::BindRepeating(&AssistantStateBase::UpdateLaunchWithMicOpen,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantNotificationEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateNotificationEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      assistant::prefs::kAssistantOnboardingMode,
      base::BindRepeating(&AssistantStateBase::UpdateOnboardingMode,
                          base::Unretained(this)));

  UpdateConsentStatus();
  UpdateContextEnabled();
  UpdateSettingsEnabled();
  UpdateHotwordAlwaysOn();
  UpdateHotwordEnabled();
  UpdateLaunchWithMicOpen();
  UpdateNotificationEnabled();
  UpdateOnboardingMode();
}

bool AssistantStateBase::IsScreenContextAllowed() const {
  return allowed_state() == assistant::AssistantAllowedState::ALLOWED &&
         settings_enabled().value_or(false) &&
         context_enabled().value_or(false);
}

bool AssistantStateBase::HasAudioInputDevice() const {
  ash::AudioDeviceList devices;
  ash::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (const AudioDevice& device : devices) {
    if (device.is_input)
      return true;
  }
  return false;
}

void AssistantStateBase::InitializeObserver(AssistantStateObserver* observer) {
  if (consent_status_.has_value())
    observer->OnAssistantConsentStatusChanged(consent_status_.value());
  if (context_enabled_.has_value())
    observer->OnAssistantContextEnabled(context_enabled_.value());
  if (settings_enabled_.has_value())
    observer->OnAssistantSettingsEnabled(settings_enabled_.value());
  if (hotword_always_on_.has_value())
    observer->OnAssistantHotwordAlwaysOn(hotword_always_on_.value());
  if (hotword_enabled_.has_value())
    observer->OnAssistantHotwordEnabled(hotword_enabled_.value());
  if (launch_with_mic_open_.has_value())
    observer->OnAssistantLaunchWithMicOpen(launch_with_mic_open_.value());
  if (notification_enabled_.has_value())
    observer->OnAssistantNotificationEnabled(notification_enabled_.value());
  if (onboarding_mode_.has_value())
    observer->OnAssistantOnboardingModeChanged(onboarding_mode_.value());

  observer->OnAssistantStatusChanged(assistant_status_);
  if (allowed_state_.has_value())
    observer->OnAssistantFeatureAllowedChanged(allowed_state_.value());
  if (locale_.has_value())
    observer->OnLocaleChanged(locale_.value());
  if (arc_play_store_enabled_.has_value())
    observer->OnArcPlayStoreEnabledChanged(arc_play_store_enabled_.value());
}

void AssistantStateBase::UpdateConsentStatus() {
  auto consent_status = pref_change_registrar_->prefs()->GetInteger(
      assistant::prefs::kAssistantConsentStatus);
  if (consent_status_.has_value() &&
      consent_status_.value() == consent_status) {
    return;
  }
  consent_status_ = consent_status;
  for (auto& observer : observers_)
    observer.OnAssistantConsentStatusChanged(consent_status_.value());
}

void AssistantStateBase::UpdateContextEnabled() {
  auto context_enabled = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantContextEnabled);
  if (context_enabled_.has_value() &&
      context_enabled_.value() == context_enabled) {
    return;
  }
  context_enabled_ = context_enabled;
  for (auto& observer : observers_)
    observer.OnAssistantContextEnabled(context_enabled_.value());
}

void AssistantStateBase::UpdateSettingsEnabled() {
  auto settings_enabled = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantEnabled);
  if (settings_enabled_.has_value() &&
      settings_enabled_.value() == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;
  for (auto& observer : observers_)
    observer.OnAssistantSettingsEnabled(settings_enabled_.value());
}

void AssistantStateBase::UpdateHotwordAlwaysOn() {
  auto hotword_always_on = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantHotwordAlwaysOn);
  if (hotword_always_on_.has_value() &&
      hotword_always_on_.value() == hotword_always_on) {
    return;
  }
  hotword_always_on_ = hotword_always_on;
  for (auto& observer : observers_)
    observer.OnAssistantHotwordAlwaysOn(hotword_always_on_.value());
}

void AssistantStateBase::UpdateHotwordEnabled() {
  auto hotword_enabled = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantHotwordEnabled);
  if (hotword_enabled_.has_value() &&
      hotword_enabled_.value() == hotword_enabled) {
    return;
  }
  hotword_enabled_ = hotword_enabled;
  for (auto& observer : observers_)
    observer.OnAssistantHotwordEnabled(hotword_enabled_.value());
}

void AssistantStateBase::UpdateLaunchWithMicOpen() {
  auto launch_with_mic_open = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantLaunchWithMicOpen);
  if (launch_with_mic_open_.has_value() &&
      launch_with_mic_open_.value() == launch_with_mic_open) {
    return;
  }
  launch_with_mic_open_ = launch_with_mic_open;
  for (auto& observer : observers_)
    observer.OnAssistantLaunchWithMicOpen(launch_with_mic_open_.value());
}

void AssistantStateBase::UpdateNotificationEnabled() {
  auto notification_enabled = pref_change_registrar_->prefs()->GetBoolean(
      assistant::prefs::kAssistantNotificationEnabled);
  if (notification_enabled_.has_value() &&
      notification_enabled_.value() == notification_enabled) {
    return;
  }
  notification_enabled_ = notification_enabled;
  for (auto& observer : observers_)
    observer.OnAssistantNotificationEnabled(notification_enabled_.value());
}

void AssistantStateBase::UpdateOnboardingMode() {
  AssistantOnboardingMode onboarding_mode = assistant::prefs::ToOnboardingMode(
      pref_change_registrar_->prefs()->GetString(
          assistant::prefs::kAssistantOnboardingMode));

  if (onboarding_mode_ == onboarding_mode)
    return;

  onboarding_mode_ = onboarding_mode;
  for (auto& observer : observers_)
    observer.OnAssistantOnboardingModeChanged(onboarding_mode_.value());
}

void AssistantStateBase::UpdateAssistantStatus(
    assistant::AssistantStatus status) {
  assistant_status_ = status;
  for (auto& observer : observers_)
    observer.OnAssistantStatusChanged(assistant_status_);
}

void AssistantStateBase::UpdateFeatureAllowedState(
    assistant::AssistantAllowedState state) {
  allowed_state_ = state;
  for (auto& observer : observers_)
    observer.OnAssistantFeatureAllowedChanged(allowed_state_.value());
}

void AssistantStateBase::UpdateLocale(const std::string& locale) {
  locale_ = locale;
  for (auto& observer : observers_)
    observer.OnLocaleChanged(locale_.value());
}

void AssistantStateBase::UpdateArcPlayStoreEnabled(bool enabled) {
  arc_play_store_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnArcPlayStoreEnabledChanged(arc_play_store_enabled_.value());
}

void AssistantStateBase::UpdateLockedFullScreenState(bool enabled) {
  locked_full_screen_enabled_ = enabled;
  for (auto& observer : observers_) {
    observer.OnLockedFullScreenStateChanged(
        locked_full_screen_enabled_.value());
  }
}

}  // namespace ash
