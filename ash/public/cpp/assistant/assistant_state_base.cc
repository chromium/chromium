// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state_base.h"

#include <ostream>
#include <sstream>

#include "ash/public/cpp/accelerators.h"
#include "base/strings/string_piece_forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {
template <typename T>
void PrintValue(std::stringstream* result,
                const std::string& name,
                const base::Optional<T>& value) {
  *result << std::endl << "  " << name << ": ";
  if (value.has_value())
    *result << value.value();
  else
    *result << ("(no value)");
}

#define PRINT_VALUE(value) PrintValue(&result, #value, value())
}  // namespace

AssistantStateBase::AssistantStateBase() = default;

AssistantStateBase::~AssistantStateBase() = default;

std::string AssistantStateBase::ToString() const {
  std::stringstream result;
  result << "AssistantState:";
  result << assistant_state_;
  PRINT_VALUE(settings_enabled);
  PRINT_VALUE(context_enabled);
  PRINT_VALUE(hotword_enabled);
  PRINT_VALUE(allowed_state);
  PRINT_VALUE(locale);
  PRINT_VALUE(arc_play_store_enabled);
  PRINT_VALUE(locked_full_screen_enabled);
  return result.str();
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
      chromeos::assistant::prefs::kAssistantConsentStatus,
      base::BindRepeating(&AssistantStateBase::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantContextEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateContextEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateSettingsEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantHotwordAlwaysOn,
      base::BindRepeating(&AssistantStateBase::UpdateHotwordAlwaysOn,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantHotwordEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateHotwordEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen,
      base::BindRepeating(&AssistantStateBase::UpdateLaunchWithMicOpen,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      chromeos::assistant::prefs::kAssistantNotificationEnabled,
      base::BindRepeating(&AssistantStateBase::UpdateNotificationEnabled,
                          base::Unretained(this)));

  UpdateConsentStatus();
  UpdateContextEnabled();
  UpdateSettingsEnabled();
  UpdateHotwordAlwaysOn();
  UpdateHotwordEnabled();
  UpdateLaunchWithMicOpen();
  UpdateNotificationEnabled();
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

  InitializeObserverMojom(observer);
}

void AssistantStateBase::InitializeObserverMojom(
    mojom::AssistantStateObserver* observer) {
  observer->OnAssistantStatusChanged(assistant_state_);
  if (allowed_state_.has_value())
    observer->OnAssistantFeatureAllowedChanged(allowed_state_.value());
  if (locale_.has_value())
    observer->OnLocaleChanged(locale_.value());
  if (arc_play_store_enabled_.has_value())
    observer->OnArcPlayStoreEnabledChanged(arc_play_store_enabled_.value());
}

void AssistantStateBase::UpdateConsentStatus() {
  auto consent_status = pref_change_registrar_->prefs()->GetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus);
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
      chromeos::assistant::prefs::kAssistantContextEnabled);
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
      chromeos::assistant::prefs::kAssistantEnabled);
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
      chromeos::assistant::prefs::kAssistantHotwordAlwaysOn);
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
      chromeos::assistant::prefs::kAssistantHotwordEnabled);
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
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen);
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
      chromeos::assistant::prefs::kAssistantNotificationEnabled);
  if (notification_enabled_.has_value() &&
      notification_enabled_.value() == notification_enabled) {
    return;
  }
  notification_enabled_ = notification_enabled;
  for (auto& observer : observers_)
    observer.OnAssistantNotificationEnabled(notification_enabled_.value());
}

void AssistantStateBase::UpdateAssistantStatus(mojom::AssistantState state) {
  assistant_state_ = state;
  for (auto& observer : observers_)
    observer.OnAssistantStatusChanged(assistant_state_);
}

void AssistantStateBase::UpdateFeatureAllowedState(
    mojom::AssistantAllowedState state) {
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
