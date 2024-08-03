// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {
namespace {

bool ShouldReenableKeyboardAmbientLightSensor(
    const AccountId& account_id,
    user_manager::KnownUser& known_user) {
  // Retrieve the reason.
  const int keyboard_ambient_light_sensor_disabled_reason =
      known_user
          .FindIntPath(account_id,
                       prefs::kKeyboardAmbientLightSensorDisabledReason)
          .value_or(static_cast<int>(
              power_manager::
                  AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP));

  // Re-enable ambient light sensor if cause is not from settings app, or
  // restored from preference.
  switch (keyboard_ambient_light_sensor_disabled_reason) {
    case power_manager::
        AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP:
    case power_manager::
        AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST_SETTINGS_APP:
    case power_manager::
        AmbientLightSensorChange_Cause_RESTORED_FROM_USER_PREFERENCE:
      return false;
    default:
      return true;
  }
}

bool ShouldRestoreKeyboardAmbientLightSensor(
    const AccountId& account_id,
    user_manager::KnownUser& known_user) {
  // Retrieve the reason.
  const int keyboard_ambient_light_sensor_disabled_reason =
      known_user
          .FindIntPath(account_id, prefs::kAmbientLightSensorDisabledReason)
          .value_or(static_cast<int>(
              power_manager::
                  AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP));

  // Only restore ALS when cause is from settings app.
  switch (keyboard_ambient_light_sensor_disabled_reason) {
    case power_manager::
        AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP:
    case power_manager::
        AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST_SETTINGS_APP:
      return true;
    default:
      return false;
  }
}

bool ShouldSaveKeyboardAmbientLightSensorForNewDevice(
    const power_manager::AmbientLightSensorChange& change) {
  return change.cause() ==
             power_manager::
                 AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP ||
         change.cause() ==
             power_manager::
                 AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST_SETTINGS_APP;
}

power_manager::SetAmbientLightSensorEnabledRequest_Cause
KeyboardAmbientLightSensorChangeSourceToCause(
    KeyboardAmbientLightSensorEnabledChangeSource source) {
  switch (source) {
    case KeyboardAmbientLightSensorEnabledChangeSource::kSettingsApp:
      return power_manager::
          SetAmbientLightSensorEnabledRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP;
    // TODO(longbowei): Add a new cause
    // SetAmbientLightSensorEnabledRequest_Cause_SYSTEM_REENABLED to platform
    // and update.
    default:
      return power_manager::
          SetAmbientLightSensorEnabledRequest_Cause_RESTORED_FROM_USER_PREFERENCE;
  }
}

power_manager::SetBacklightBrightnessRequest_Cause
KeyboardBrightnessChangeSourceToCause(KeyboardBrightnessChangeSource source) {
  switch (source) {
    case KeyboardBrightnessChangeSource::kSettingsApp:
      return power_manager::
          SetBacklightBrightnessRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP;
    case KeyboardBrightnessChangeSource::kRestoredFromUserPref:
      return power_manager::
          SetBacklightBrightnessRequest_Cause_RESTORED_FROM_USER_PREFERENCE;
    default:
      return power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST;
  }
}

}  // namespace

KeyboardBrightnessController::KeyboardBrightnessController(
    PrefService* local_state,
    SessionControllerImpl* session_controller)
    : local_state_(local_state), session_controller_(session_controller) {
  // Add SessionController observer.
  DCHECK(session_controller_);
  session_controller_->AddObserver(this);

  // Add PowerManagerClient observer
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  power_manager_client->AddObserver(this);

  // Record whether the keyboard has a backlight for metric collection.
  power_manager_client->HasKeyboardBacklight(base::BindOnce(
      &KeyboardBrightnessController::OnReceiveHasKeyboardBacklight,
      weak_ptr_factory_.GetWeakPtr()));

  // Record whether the device has a ambient light sensor for metric collection.
  power_manager_client->HasAmbientLightSensor(base::BindOnce(
      &KeyboardBrightnessController::OnReceiveHasAmbientLightSensor,
      weak_ptr_factory_.GetWeakPtr()));

  // Add LoginScreenController observer.
  Shell::Get()->login_screen_controller()->data_dispatcher()->AddObserver(this);
}

KeyboardBrightnessController::~KeyboardBrightnessController() {
  // Remove SessionController observer.
  DCHECK(session_controller_);
  session_controller_->RemoveObserver(this);

  // Remove PowerManagerClient observer
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  // Remove LoginScreenController observer if exists.
  LoginScreenController* login_screen_controller =
      Shell::Get()->login_screen_controller();
  LoginDataDispatcher* data_dispatcher =
      login_screen_controller ? login_screen_controller->data_dispatcher()
                              : nullptr;
  if (data_dispatcher) {
    // Remove this observer to prevent dangling pointer errors that can occur
    // in scenarios where accelerator_controller_unittest.cc reassigns Shell's
    // brightness_control_delegate_.
    data_dispatcher->RemoveObserver(this);
  }
}

// static:
void KeyboardBrightnessController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kKeyboardAmbientLightSensorLastEnabled,
      /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// SessionObserver:
void KeyboardBrightnessController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;

  // On login, retrieve the current keyboard brightness and save it to prefs.
  HandleGetKeyboardBrightness(base::BindOnce(
      &KeyboardBrightnessController::OnReceiveKeyboardBrightnessAfterLogin,
      weak_ptr_factory_.GetWeakPtr()));
}

// SessionObserver:
void KeyboardBrightnessController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_service_ = pref_service;

  // Don't restore the ambient light sensor value if the relevant flag is
  // disabled.
  if (!features::IsKeyboardBacklightControlInSettingsEnabled()) {
    return;
  }

  // Only restore the profile-synced ambient light sensor setting if it's a
  // user's first time logging in to a new device.
  if (!session_controller_->IsUserFirstLogin()) {
    return;
  }

  // Observe the state of the synced profile pref so that the keyboard ambient
  // light sensor setting will be restored as soon as the pref finishes syncing
  // on the new device.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  if (pref_service_) {
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        prefs::kKeyboardAmbientLightSensorLastEnabled,
        base::BindRepeating(
            &KeyboardBrightnessController::
                RestoreKeyboardAmbientLightSensorSettingOnFirstLogin,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

// PowerManagerClient::Observer:
void KeyboardBrightnessController::KeyboardAmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& change) {
  // In tests and during OOBE, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  user_manager::KnownUser known_user(local_state_);

  // If the keyboard ambient light sensor was disabled, save the cause for that
  // change into a KnownUser pref. This pref can be used if we need to
  // systematically re-enable the ambient light sensor for a subset of users
  // (e.g. those who didn't manually disable the sensor from the Settings app).
  if (!change.sensor_enabled()) {
    known_user.SetPath(
        active_account_id_.value(),
        prefs::kKeyboardAmbientLightSensorDisabledReason,
        std::make_optional<base::Value>(static_cast<int>(change.cause())));
    keyboard_ambient_light_sensor_disabled_timestamp_ = base::Time::Now();
  } else {
    // If the ambient light sensor was enabled, remove the existing "disabled
    // reason" pref.
    known_user.RemovePref(active_account_id_.value(),
                          prefs::kKeyboardAmbientLightSensorDisabledReason);
  }

  // Save the current ambient light sensor enabled status into local state.
  known_user.SetPath(active_account_id_.value(),
                     prefs::kKeyboardAmbientLightSensorEnabled,
                     std::make_optional<base::Value>(change.sensor_enabled()));

  if (ShouldSaveKeyboardAmbientLightSensorForNewDevice(change)) {
    // Save a user pref new device if change is from settings app so that we can
    // restore users' ALS settings when they login to a new device.
    PrefService* primary_user_prefs =
        session_controller_->GetActivePrefService();
    if (primary_user_prefs) {
      primary_user_prefs->SetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled,
          change.sensor_enabled());
    }
  }
}

// PowerManagerClient::Observer:
void KeyboardBrightnessController::KeyboardBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  // In tests, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  // Save keyboard brightness change to Local State if it was caused by a user
  // request.
  if (change.cause() ==
          power_manager::BacklightBrightnessChange_Cause_USER_REQUEST ||
      change.cause() ==
          power_manager::
              BacklightBrightnessChange_Cause_USER_REQUEST_FROM_SETTINGS_APP) {
    user_manager::KnownUser known_user(local_state_);
    known_user.SetPath(active_account_id_.value(),
                       prefs::kKeyboardBrightnessPercent,
                       std::make_optional<base::Value>(change.percent()));
  }
}

// PowerManagerClient::Observer:
void KeyboardBrightnessController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (!features::IsKeyboardBacklightControlInSettingsEnabled()) {
    return;
  }
  // In tests, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }
  if (!keyboard_ambient_light_sensor_disabled_timestamp_.has_value()) {
    return;
  }

  user_manager::KnownUser known_user(local_state_);
  base::Time now = base::Time::Now();

  // Re-enable ALS if it passed local midnight.
  if (now.LocalMidnight() -
          keyboard_ambient_light_sensor_disabled_timestamp_.value()
              .LocalMidnight() >=
      base::Days(1)) {
    if (ShouldReenableKeyboardAmbientLightSensor(active_account_id_.value(),
                                                 known_user)) {
      HandleSetKeyboardAmbientLightSensorEnabled(
          true,
          KeyboardAmbientLightSensorEnabledChangeSource::kSystemReenabled);
    }
  }
}

// LoginDataDispatcher::Observer:
void KeyboardBrightnessController::OnFocusPod(const AccountId& account_id) {
  active_account_id_ = account_id;

  if (features::IsKeyboardBacklightControlInSettingsEnabled()) {
    RestoreKeyboardBrightnessSettings(account_id);
  }
}

void KeyboardBrightnessController::HandleKeyboardBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
}

void KeyboardBrightnessController::HandleToggleKeyboardBacklight() {
  chromeos::PowerManagerClient::Get()->ToggleKeyboardBacklight();
}

void KeyboardBrightnessController::HandleSetKeyboardBrightness(
    double percent,
    bool gradual,
    KeyboardBrightnessChangeSource source) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_FAST
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(KeyboardBrightnessChangeSourceToCause(source));
  chromeos::PowerManagerClient::Get()->SetKeyboardBrightness(request);
}

void KeyboardBrightnessController::HandleGetKeyboardAmbientLightSensorEnabled(
    base::OnceCallback<void(std::optional<bool>)> callback) {
  chromeos::PowerManagerClient::Get()->GetKeyboardAmbientLightSensorEnabled(
      std::move(callback));
}

void KeyboardBrightnessController::HandleGetKeyboardBrightness(
    base::OnceCallback<void(std::optional<double>)> callback) {
  chromeos::PowerManagerClient::Get()->GetKeyboardBrightnessPercent(
      std::move(callback));
}

void KeyboardBrightnessController::HandleSetKeyboardAmbientLightSensorEnabled(
    bool enabled,
    KeyboardAmbientLightSensorEnabledChangeSource source) {
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(enabled);
  request.set_cause(KeyboardAmbientLightSensorChangeSourceToCause(source));
  chromeos::PowerManagerClient::Get()->SetKeyboardAmbientLightSensorEnabled(
      request);
}

void KeyboardBrightnessController::RestoreKeyboardBrightnessSettings(
    const AccountId& account_id) {
  user_manager::KnownUser known_user(local_state_);
  bool keyboard_ambient_light_sensor_enabled_for_account = true;

  if (ShouldReenableKeyboardAmbientLightSensor(account_id, known_user)) {
    HandleSetKeyboardAmbientLightSensorEnabled(
        /*enabled=*/true,
        KeyboardAmbientLightSensorEnabledChangeSource::kSystemReenabled);
  } else {
    keyboard_ambient_light_sensor_enabled_for_account =
        known_user
            .FindBoolPath(account_id, prefs::kKeyboardAmbientLightSensorEnabled)
            .value_or(true);
    if (!keyboard_ambient_light_sensor_enabled_for_account) {
      // If the keyboard ambient light sensor is disabled, restore the user's
      // preferred keyboard brightness level.
      const std::optional<double> keyboard_brightness_for_account =
          known_user.FindPath(account_id, prefs::kKeyboardBrightnessPercent)
              ->GetIfDouble();
      if (keyboard_brightness_for_account.has_value()) {
        HandleSetKeyboardBrightness(
            keyboard_brightness_for_account.value(),
            /*gradual=*/true,
            KeyboardBrightnessChangeSource::kRestoredFromUserPref);
      }
    }
    if (ShouldRestoreKeyboardAmbientLightSensor(account_id, known_user)) {
      HandleSetKeyboardAmbientLightSensorEnabled(
          keyboard_ambient_light_sensor_enabled_for_account,
          KeyboardAmbientLightSensorEnabledChangeSource::kRestoredFromUserPref);
    }
  }

  // Record the keyboard ambient light sensor status at login.
  if (has_sensor_ && !has_keyboard_ambient_light_sensor_status_been_recorded_) {
    base::UmaHistogramBoolean(
        "ChromeOS.Keyboard.Startup.AmbientLightSensorEnabled",
        keyboard_ambient_light_sensor_enabled_for_account);
    has_keyboard_ambient_light_sensor_status_been_recorded_ = true;
  }
}

void KeyboardBrightnessController::
    RestoreKeyboardAmbientLightSensorSettingOnFirstLogin() {
  if (!features::IsKeyboardBacklightControlInSettingsEnabled() ||
      !pref_service_ ||
      has_keyboard_ambient_light_sensor_been_restored_for_new_user_) {
    return;
  }

  // Restore the keyboard ambient light sensor setting.
  const bool ambient_light_sensor_last_enabled_for_account =
      pref_service_->GetBoolean(prefs::kKeyboardAmbientLightSensorLastEnabled);
  HandleSetKeyboardAmbientLightSensorEnabled(
      ambient_light_sensor_last_enabled_for_account,
      KeyboardAmbientLightSensorEnabledChangeSource::kRestoredFromUserPref);

  has_keyboard_ambient_light_sensor_been_restored_for_new_user_ = true;
}

void KeyboardBrightnessController::OnReceiveHasKeyboardBacklight(
    std::optional<bool> has_keyboard_backlight) {
  if (has_keyboard_backlight.has_value()) {
    base::UmaHistogramBoolean("ChromeOS.Keyboard.HasBacklight",
                              has_keyboard_backlight.value());
    return;
  }
  LOG(ERROR) << "KeyboardBrightnessController: Failed to get the keyboard "
                "backlight status";
}

void KeyboardBrightnessController::OnReceiveHasAmbientLightSensor(
    std::optional<bool> has_sensor) {
  if (!has_sensor.has_value()) {
    LOG(ERROR)
        << "KeyboardBrightnessController: Failed to get the ambient light "
           "sensor status";
    return;
  }
  has_sensor_ = has_sensor.value();
  base::UmaHistogramBoolean("ChromeOS.Keyboard.HasAmbientLightSensor",
                            has_sensor.value());
}

void KeyboardBrightnessController::OnReceiveKeyboardBrightnessAfterLogin(
    std::optional<double> keyboard_brightness) {
  // In tests, these may not be present.
  if (!active_account_id_.has_value() || !local_state_) {
    return;
  }

  if (!keyboard_brightness.has_value()) {
    LOG(ERROR) << "KeyboardBrightnessController: keyboard_brightness has no "
                  "value, so cannot set prefs.";
    return;
  }

  // Save keyboard brightness to local state after login.
  user_manager::KnownUser known_user(local_state_);
  known_user.SetPath(
      active_account_id_.value(), prefs::kKeyboardBrightnessPercent,
      std::make_optional<base::Value>(keyboard_brightness.value()));
}

}  // namespace ash
