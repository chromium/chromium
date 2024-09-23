// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/privacy_screen_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

namespace {

// Gets the DisplaySnapshot of the internal display that supports privacy
// screen. Returns nullptr if none exists.
display::DisplaySnapshot* GetSupportedDisplay() {
  const auto& cached_displays =
      Shell::Get()->display_configurator()->cached_displays();

  for (display::DisplaySnapshot* display : cached_displays) {
    if (display->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL &&
        display->privacy_screen_state() != display::kNotSupported &&
        display->current_mode()) {
      return display;
    }
  }
  return nullptr;
}

// Gets the ID of the internal display that supports privacy screen. Returns
// display::kInvalidDisplayId if none is found.
int64_t GetSupportedDisplayId() {
  auto* privacy_screen_display = GetSupportedDisplay();
  return privacy_screen_display ? privacy_screen_display->display_id()
                                : display::kInvalidDisplayId;
}

}  // namespace

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
  return current_status_;
}

void PrivacyScreenController::SetEnabled(bool enabled) {
  if (!IsSupported()) {
    LOG(ERROR) << "Attempted to set privacy-screen on an unsupported device.";
    return;
  }

  // Do not set the policy if it is managed by policy. However, we still want to
  // notify observers that a change was attempted in order to show a toast.
  if (IsManaged()) {
    const bool currently_enabled = GetEnabled();
    for (Observer& observer : observers_)
      observer.OnPrivacyScreenSettingChanged(currently_enabled,
                                             /*notify_ui=*/true);
    return;
  }

  if (active_user_pref_service_) {
    if (GetStateFromActiveUserPreference() == enabled) {
      // Since it is possible for DRM to fail a privacy screen hardware toggle,
      // following calls to SetEnabled() may try to set the user pref to a state
      // it is already set to. This will end up as a NOP for such SetEnabled()
      // calls. Therefore, we manually trigger a call to OnStateChanged here to
      // allow following toggle attempts to get through to DRM.
      OnStateChanged(/*from_user_pref_init=*/false);
    } else {
      active_user_pref_service_->SetBoolean(prefs::kDisplayPrivacyScreenEnabled,
                                            enabled);
    }
  }
}

void PrivacyScreenController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacyScreenController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PrivacyScreenController::SetEnforced(bool enforced) {
  // Only send a toast to the user if policy has changed the state of the
  // privacy screen.
  dlp_enforced_ = enforced;
  OnStateChanged(/*from_user_pref_init=*/false);
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

void PrivacyScreenController::OnDisplayConfigurationChanged(
    const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
        displays) {
  // OnDisplayConfigurationChanged() may fire many times during Chrome's
  // lifetime. We limit automatic user pref initialization to login screen only.
  if (!applying_login_screen_prefs_)
    return;

  // Extract the initial state of the privacy screen from the supporting panel
  // at the time the display was configured.
  display::DisplaySnapshot* privacy_screen_display = GetSupportedDisplay();
  current_status_ =
      privacy_screen_display &&
      (privacy_screen_display->privacy_screen_state() == display::kEnabled ||
       privacy_screen_display->privacy_screen_state() ==
           display::kEnabledLocked);

  InitFromUserPrefs();
  applying_login_screen_prefs_ = false;
}

bool PrivacyScreenController::CalculateCurrentStatus() const {
  if (!active_user_pref_service_)
    return dlp_enforced_;
  const bool actual_user_pref = GetStateFromActiveUserPreference();
  // If managed by policy, return the pref value.
  if (active_user_pref_service_->IsManagedPreference(
          prefs::kDisplayPrivacyScreenEnabled)) {
    return actual_user_pref;
  }
  // Otherwise return true if enforced by DLP or return the last state set by
  // the user.
  return dlp_enforced_ || actual_user_pref;
}

void PrivacyScreenController::OnStateChanged(bool from_user_pref_init) {
  const int64_t display_id = GetSupportedDisplayId();
  if (display_id == display::kInvalidDisplayId)
    return;

  const bool enable_screen = CalculateCurrentStatus();
  if (enable_screen == current_status_)
    return;

  Shell::Get()->display_configurator()->SetPrivacyScreen(
      display_id, enable_screen,
      base::BindOnce(&PrivacyScreenController::OnSetPrivacyScreenComplete,
                     weak_ptr_factory_.GetWeakPtr(), from_user_pref_init,
                     enable_screen));
}

void PrivacyScreenController::OnSetPrivacyScreenComplete(
    bool from_user_pref_init,
    bool requested_config,
    bool success) {
  if (success) {
    current_status_ = requested_config;
  } else {
    LOG(ERROR) << "Turning privacy screen " << (requested_config ? "ON" : "OFF")
               << " was unsuccessful.";
  }

  const bool notify_observers = ShouldNotifyObservers(from_user_pref_init);
  for (Observer& observer : observers_)
    observer.OnPrivacyScreenSettingChanged(current_status_, notify_observers);
}

void PrivacyScreenController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDisplayPrivacyScreenEnabled,
      base::BindRepeating(&PrivacyScreenController::OnStateChanged,
                          weak_ptr_factory_.GetWeakPtr(),
                          /*from_user_pref_init=*/false));

  OnStateChanged(/*from_user_pref_init=*/true);
}

bool PrivacyScreenController::GetStateFromActiveUserPreference() const {
  return active_user_pref_service_ && active_user_pref_service_->GetBoolean(
                                          prefs::kDisplayPrivacyScreenEnabled);
}

bool PrivacyScreenController::ShouldNotifyObservers(
    bool from_user_pref_init) const {
  // We don't want to notify observers upon initialization or on account change
  // because changes will trigger a toast to show up.
  return !from_user_pref_init;
}

}  // namespace ash
