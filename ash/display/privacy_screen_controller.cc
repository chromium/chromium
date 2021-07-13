// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/privacy_screen_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

PrivacyScreenController::PrivacyScreenController() {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->display_configurator()->AddObserver(this);
}

PrivacyScreenController::~PrivacyScreenController() {
  Shell::Get()->display_configurator()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PrivacyScreenController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisplayPrivacyScreenEnabled, false);
}

bool PrivacyScreenController::IsSupported() const {
  return GetSupportedDisplayId() != display::kInvalidDisplayId;
}

bool PrivacyScreenController::IsManaged() const {
  return dlp_enforced_ || (active_user_pref_service_ &&
                           active_user_pref_service_->IsManagedPreference(
                               prefs::kDisplayPrivacyScreenEnabled));
}

bool PrivacyScreenController::GetEnabled() const {
  if (!active_user_pref_service_)
    return dlp_enforced_;
  const bool actual_user_pref = active_user_pref_service_->GetBoolean(
      prefs::kDisplayPrivacyScreenEnabled);
  // If managed by policy, return the pref value.
  if (active_user_pref_service_->IsManagedPreference(
          prefs::kDisplayPrivacyScreenEnabled)) {
    return actual_user_pref;
  }
  // Otherwise return true if enforced by DLP or return the last state set by
  // the user.
  return dlp_enforced_ || actual_user_pref;
}

void PrivacyScreenController::SetEnabled(bool enabled,
                                         ToggleUISurface ui_surface) {
  if (!IsSupported()) {
    LOG(ERROR) << "Attempted to set privacy-screen on an unsupported device.";
    return;
  }

  // Do not set the policy if it is managed by policy. However, we still want to
  // notify observers that a change was attempted in order to show a toast.
  if (IsManaged()) {
    const bool currently_enabled = GetEnabled();
    for (Observer& observer : observers_)
      observer.OnPrivacyScreenSettingChanged(currently_enabled);
    return;
  }

  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kDisplayPrivacyScreenEnabled,
                                          enabled);
  }

  if (ui_surface == kToggleUISurfaceCount)
    return;

  if (enabled) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.PrivacyScreen.Toggled.Enabled",
                              ui_surface, kToggleUISurfaceCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.PrivacyScreen.Toggled.Disabled",
                              ui_surface, kToggleUISurfaceCount);
  }
}

void PrivacyScreenController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacyScreenController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PrivacyScreenController::SetEnforced(bool enforced) {
  dlp_enforced_ = enforced;
  OnStateChanged(true);
}

void PrivacyScreenController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void PrivacyScreenController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  active_user_pref_service_ = prefs;

  // The privacy screen is toggled via commands to the GPU process, which is
  // initialized after the signin screen emits this event. Therefore we must
  // wait for OnDisplayModeChanged() to notify us when the display configuration
  // is ready, which implies that the GPU process and communication pipes are
  // ready.
  applying_login_screen_prefs_ = true;
}

void PrivacyScreenController::OnDisplayModeChanged(
    const std::vector<display::DisplaySnapshot*>& displays) {
  // OnDisplayModeChanged() may fire many times during Chrome's lifetime. We
  // limit automatic user pref initialization to login screen only.
  if (applying_login_screen_prefs_) {
    InitFromUserPrefs();
    applying_login_screen_prefs_ = false;
  }
}

void PrivacyScreenController::OnStateChanged(bool notify_observers) {
  const int64_t display_id = GetSupportedDisplayId();
  if (display_id == display::kInvalidDisplayId)
    return;

  const bool is_enabled = GetEnabled();
  Shell::Get()->display_configurator()->SetPrivacyScreen(display_id,
                                                         is_enabled);

  if (!notify_observers)
    return;

  for (Observer& observer : observers_)
    observer.OnPrivacyScreenSettingChanged(is_enabled);
}

void PrivacyScreenController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDisplayPrivacyScreenEnabled,
      base::BindRepeating(&PrivacyScreenController::OnStateChanged,
                          base::Unretained(this),
                          /*notify_observers=*/true));

  // We don't want to notify observers upon initialization or on account change
  // because changes will trigger a toast to show up.
  OnStateChanged(/*notify_observers=*/false);
}

int64_t PrivacyScreenController::GetSupportedDisplayId() const {
  const auto& cached_displays =
      Shell::Get()->display_configurator()->cached_displays();

  for (auto* display : cached_displays) {
    if (display->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL &&
        display->privacy_screen_state() != display::kNotSupported &&
        display->current_mode()) {
      return display->display_id();
    }
  }

  return display::kInvalidDisplayId;
}

}  // namespace ash
