// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/power/power_status.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"

namespace ash::system {

namespace {

std::string GetBrightnessActionName(BrightnessAction brightness_action) {
  switch (brightness_action) {
    case BrightnessAction::kDecreaseBrightness:
      return "Decrease";
    case BrightnessAction::kIncreaseBrightness:
      return "Increase";
    case BrightnessAction::kSetBrightness:
      return "Set";
  }
}

// Returns true if the device is currently connected to a charger.
// Note: This is the same logic that ambient_controller.cc uses.
bool IsChargerConnected() {
  DCHECK(PowerStatus::IsInitialized());
  auto* power_status = PowerStatus::Get();
  if (power_status->IsBatteryPresent()) {
    // If battery is charging, that implies sufficient power is connected. If
    // battery is not charging, return true only if an official, non-USB charger
    // is connected. This will happen if the battery is fully charged or
    // charging is delayed by Adaptive Charging.
    return power_status->IsBatteryCharging() ||
           power_status->IsMainsChargerConnected();
  }

  // Chromeboxes have no battery.
  return power_status->IsLinePowerConnected();
}

void SaveBrightnessPercentToLocalState(PrefService* local_state,
                                       const AccountId& account_id,
                                       double percent) {
  user_manager::KnownUser known_user(local_state);
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(percent));
}

}  // namespace

BrightnessControllerChromeos::BrightnessControllerChromeos(
    PrefService* local_state,
    SessionControllerImpl* session_controller)
    : local_state_(local_state), session_controller_(session_controller) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  DCHECK(session_controller_);
  session_controller_->AddObserver(this);

  Shell::Get()->login_screen_controller()->data_dispatcher()->AddObserver(this);

  // Record a timestamp when this is constructed so last_session_change_time_ is
  // guaranteed to have a value.
  last_session_change_time_ = base::TimeTicks::Now();
}

BrightnessControllerChromeos::~BrightnessControllerChromeos() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  DCHECK(session_controller_);
  session_controller_->RemoveObserver(this);

  LoginScreenController* login_screen_controller =
      Shell::Get()->login_screen_controller();
  LoginDataDispatcher* data_dispatcher =
      login_screen_controller ? login_screen_controller->data_dispatcher()
                              : nullptr;
  // Typically, brightness_control_delegate is destroyed after
  // login_screen_controller in shell.cc (which is why we check to see if
  // login_screen_controller still exists), so it's not necessary to remove the
  // observer. However, accelerator_controller_unittest.cc reassigns Shell's
  // brightness_control_delegate_, which causes a dangling raw_ptr error unless
  // the observer is removed here.
  if (data_dispatcher) {
    data_dispatcher->RemoveObserver(this);
  }
}

// static
void BrightnessControllerChromeos::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDisplayAmbientLightSensorLastEnabled,
      /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void BrightnessControllerChromeos::HandleBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseScreenBrightness(true);
  RecordHistogramForBrightnessAction(BrightnessAction::kDecreaseBrightness);
}

void BrightnessControllerChromeos::HandleBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseScreenBrightness();
  RecordHistogramForBrightnessAction(BrightnessAction::kIncreaseBrightness);
}

void BrightnessControllerChromeos::SetBrightnessPercent(
    double percent,
    bool gradual,
    BrightnessChangeSource source) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_FAST
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  power_manager::SetBacklightBrightnessRequest_Cause brightness_change_cause =
      source == BrightnessChangeSource::kSettingsApp
          ? power_manager::
                SetBacklightBrightnessRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP
          : power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST;
  request.set_cause(brightness_change_cause);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
  RecordHistogramForBrightnessAction(BrightnessAction::kSetBrightness);
}

void BrightnessControllerChromeos::GetBrightnessPercent(
    base::OnceCallback<void(std::optional<double>)> callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      std::move(callback));
}

void BrightnessControllerChromeos::SetAmbientLightSensorEnabled(bool enabled) {
  chromeos::PowerManagerClient::Get()->SetAmbientLightSensorEnabled(enabled);
}

void BrightnessControllerChromeos::GetAmbientLightSensorEnabled(
    base::OnceCallback<void(std::optional<bool>)> callback) {
  chromeos::PowerManagerClient::Get()->GetAmbientLightSensorEnabled(
      std::move(callback));
}

void BrightnessControllerChromeos::HasAmbientLightSensor(
    base::OnceCallback<void(std::optional<bool>)> callback) {
  chromeos::PowerManagerClient::Get()->HasAmbientLightSensor(
      std::move(callback));
}

void BrightnessControllerChromeos::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Whenever the SessionState changes (e.g. LOGIN_PRIMARY to ACTIVE), record
  // the timestamp.
  last_session_change_time_ = base::TimeTicks::Now();
}

void BrightnessControllerChromeos::OnFocusPod(const AccountId& account_id) {
  active_account_id_ = account_id;

  if (features::IsBrightnessControlInSettingsEnabled()) {
    RestoreBrightnessSettings(account_id);
  }
}

void BrightnessControllerChromeos::RestoreBrightnessSettings(
    const AccountId& account_id) {
  // Get the user's stored preference for whether the ambient light sensor
  // should be enabled. If there is no saved preference for the ambient light
  // sensor value, set the ambient light sensor to be enabled to match the
  // default behavior.
  user_manager::KnownUser known_user(local_state_);
  const bool ambient_light_sensor_enabled_for_account =
      known_user
          .FindBoolPath(account_id, prefs::kDisplayAmbientLightSensorEnabled)
          .value_or(true);

  if (!ambient_light_sensor_enabled_for_account) {
    // If the ambient light sensor is disabled, restore the user's preferred
    // brightness level.
    const std::optional<double> brightness_for_account =
        known_user
            .FindPath(account_id,
                      prefs::kInternalDisplayScreenBrightnessPercent)
            ->GetIfDouble();
    if (brightness_for_account.has_value()) {
      SetBrightnessPercent(brightness_for_account.value(), /*gradual=*/true,
                           BrightnessControlDelegate::BrightnessChangeSource::
                               kRestoredFromUserPref);
    }
  }

  SetAmbientLightSensorEnabled(ambient_light_sensor_enabled_for_account);
}

void BrightnessControllerChromeos::RestoreBrightnessSettingsOnFirstLogin() {
  // Don't restore the ambient light sensor value if the relevant flag is
  // disabled.
  if (!features::IsBrightnessControlInSettingsEnabled()) {
    return;
  }

  if (!active_pref_service_) {
    return;
  }

  // If the ambient light sensor status has already been restored, don't restore
  // it again for this device.
  if (has_ambient_light_sensor_been_restored_for_new_user_) {
    return;
  }

  // This pref has a value of true by default.
  const bool ambient_light_sensor_previously_enabled_for_account =
      active_pref_service_->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled);

  SetAmbientLightSensorEnabled(
      ambient_light_sensor_previously_enabled_for_account);

  has_ambient_light_sensor_been_restored_for_new_user_ = true;
}

void BrightnessControllerChromeos::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_pref_service_ = pref_service;

  // Don't restore the ambient light sensor value if the relevant flag is
  // disabled.
  if (!features::IsBrightnessControlInSettingsEnabled()) {
    return;
  }

  // Only restore the profile-synced ambient light sensor setting if it's a
  // user's first time logging in to a new device.
  if (!session_controller_->IsUserFirstLogin()) {
    return;
  }

  // Observe the state of the synced profile pref so that the ambient light
  // sensor setting will be restored as soon as the pref finishes syncing on the
  // new device.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  if (active_pref_service_) {
    pref_change_registrar_->Init(active_pref_service_);
    pref_change_registrar_->Add(
        prefs::kDisplayAmbientLightSensorLastEnabled,
        base::BindRepeating(&BrightnessControllerChromeos::
                                RestoreBrightnessSettingsOnFirstLogin,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void BrightnessControllerChromeos::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;

  // On login, retrieve the current brightness and save it to prefs.
  GetBrightnessPercent(
      base::BindOnce(&BrightnessControllerChromeos::OnGetBrightnessAfterLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrightnessControllerChromeos::OnGetBrightnessAfterLogin(
    std::optional<double> brightness_percent) {
  // In tests, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  if (!brightness_percent.has_value()) {
    LOG(ERROR) << "BrightnessControllerChromeos: brightness_percent has no "
                  "value, so cannot set prefs.";
    return;
  }

  SaveBrightnessPercentToLocalState(local_state_, active_account_id_.value(),
                                    brightness_percent.value());
}

void BrightnessControllerChromeos::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  // In tests, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  // Save brightness change to Local State if it was caused by a user request.
  if (change.cause() ==
          power_manager::BacklightBrightnessChange_Cause_USER_REQUEST ||
      change.cause() ==
          power_manager::
              BacklightBrightnessChange_Cause_USER_REQUEST_FROM_SETTINGS_APP) {
    SaveBrightnessPercentToLocalState(local_state_, active_account_id_.value(),
                                      change.percent());
  }
}

void BrightnessControllerChromeos::AmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& change) {
  // In tests and during OOBE, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  // If the ambient light sensor was disabled, save the cause for that change
  // into a KnownUser pref. This pref can be used if we need to systematically
  // re-enable the ambient light sensor for a subset of users (e.g. those who
  // didn't manually disable the sensor from the Settings app).
  user_manager::KnownUser known_user(local_state_);
  if (!change.sensor_enabled()) {
    known_user.SetPath(
        active_account_id_.value(), prefs::kAmbientLightSensorDisabledReason,
        std::make_optional<base::Value>(static_cast<int>(change.cause())));
  } else {
    // If the ambient light sensor was enabled, remove the existing "disabled
    // reason" pref.
    known_user.RemovePref(active_account_id_.value(),
                          prefs::kAmbientLightSensorDisabledReason);
  }

  // Save the current ambient light sensor enabled status into local state.
  known_user.SetPath(active_account_id_.value(),
                     prefs::kDisplayAmbientLightSensorEnabled,
                     std::make_optional<base::Value>(change.sensor_enabled()));

  PrefService* primary_user_prefs = session_controller_->GetActivePrefService();
  if (primary_user_prefs) {
    primary_user_prefs->SetBoolean(prefs::kDisplayAmbientLightSensorLastEnabled,
                                   change.sensor_enabled());
  }
}

void BrightnessControllerChromeos::RecordHistogramForBrightnessAction(
    BrightnessAction brightness_action) {
  // Only record the first brightness adjustment (resets on reboot).
  if (has_brightness_been_adjusted_) {
    return;
  }
  has_brightness_been_adjusted_ = true;

  CHECK(!last_session_change_time_.is_null());

  const base::TimeDelta time_since_last_session_change =
      base::TimeTicks::Now() - last_session_change_time_;

  // Don't record a metric if the first brightness adjustment occurred >1 hour
  // after the last session change.
  if (time_since_last_session_change.InHours() >= 1) {
    return;
  }

  const session_manager::SessionState session_state =
      session_controller_->GetSessionState();
  const bool is_on_login_screen =
      session_state == session_manager::SessionState::LOGIN_PRIMARY ||
      session_state == session_manager::SessionState::LOGIN_SECONDARY;
  const bool is_active_session =
      session_state == session_manager::SessionState::ACTIVE;

  // Disregard brightness events that don't occur on the login screen or in an
  // active user session.
  if (!(is_on_login_screen || is_active_session)) {
    return;
  }

  base::UmaHistogramLongTimes100(
      base::StrCat({"ChromeOS.Display.TimeUntilFirstBrightnessChange.",
                    is_on_login_screen ? "OnLoginScreen" : "AfterLogin", ".",
                    GetBrightnessActionName(brightness_action), "Brightness.",
                    IsChargerConnected() ? "Charger" : "Battery", "Power"}),
      time_since_last_session_change);
}

}  // namespace ash::system
