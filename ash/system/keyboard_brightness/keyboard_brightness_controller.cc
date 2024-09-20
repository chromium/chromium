// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
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
          .FindIntPath(account_id,
                       prefs::kKeyboardAmbientLightSensorDisabledReason)
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

std::string GetBrightnessActionName(BrightnessAction brightness_action) {
  switch (brightness_action) {
    case BrightnessAction::kDecreaseBrightness:
      return "Decrease";
    case BrightnessAction::kIncreaseBrightness:
      return "Increase";
    case BrightnessAction::kToggleBrightness:
      return "Toggle";
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

  // Record a timestamp when this is constructed so last_session_change_time_ is
  // guaranteed to have a value.
  last_session_change_time_ = base::TimeTicks::Now();
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

// SessionObserver:
void KeyboardBrightnessController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Whenever the SessionState changes (e.g. LOGIN_PRIMARY to ACTIVE), record
  // the timestamp.
  last_session_change_time_ = base::TimeTicks::Now();
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
        *active_account_id_, prefs::kKeyboardAmbientLightSensorDisabledReason,
        std::make_optional<base::Value>(static_cast<int>(change.cause())));
  } else {
    // If the ambient light sensor was enabled, remove the existing "disabled
    // reason" pref.
    known_user.RemovePref(*active_account_id_,
                          prefs::kKeyboardAmbientLightSensorDisabledReason);
  }

  // Save the current ambient light sensor enabled status into local state.
  known_user.SetPath(*active_account_id_,
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
    known_user.SetPath(*active_account_id_, prefs::kKeyboardBrightnessPercent,
                       std::make_optional<base::Value>(change.percent()));
  }
}

// LoginDataDispatcher::Observer:
void KeyboardBrightnessController::OnFocusPod(const AccountId& account_id) {
  active_account_id_ = account_id;

  if (!features::IsKeyboardBacklightControlInSettingsEnabled()) {
    return;
  }

  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (session_state == session_manager::SessionState::LOGIN_PRIMARY ||
      session_state == session_manager::SessionState::LOGIN_SECONDARY) {
    // Restore brightness settings only when device reboots.
    MaybeRestoreKeyboardBrightnessSettings();
  }
}

void KeyboardBrightnessController::HandleKeyboardBrightnessDown() {
  chromeos::PowerManagerClient::Get()->DecreaseKeyboardBrightness();
  RecordHistogramForBrightnessAction(BrightnessAction::kDecreaseBrightness);
}

void KeyboardBrightnessController::HandleKeyboardBrightnessUp() {
  chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
  RecordHistogramForBrightnessAction(BrightnessAction::kIncreaseBrightness);
}

void KeyboardBrightnessController::HandleToggleKeyboardBacklight() {
  chromeos::PowerManagerClient::Get()->ToggleKeyboardBacklight();
  RecordHistogramForBrightnessAction(BrightnessAction::kToggleBrightness);
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

  // Record the brightness action only if it was not initiated by the system's
  // brightness restoration.
  if (source != KeyboardBrightnessChangeSource::kRestoredFromUserPref) {
    RecordHistogramForBrightnessAction(BrightnessAction::kSetBrightness);
  }
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

void KeyboardBrightnessController::MaybeRestoreKeyboardBrightnessSettings() {
  if (!active_account_id_.has_value() || !has_keyboard_backlight_.has_value() ||
      !has_sensor_.has_value()) {
    return;
  }

  if (*has_keyboard_backlight_) {
    RestoreKeyboardBrightnessSettings(*active_account_id_);
  }
}

void KeyboardBrightnessController::RestoreKeyboardBrightnessSettings(
    const AccountId& account_id) {
  // In tests, local_state_ may not be present.
  if (!local_state_) {
    return;
  }
  user_manager::KnownUser known_user(local_state_);
  const std::optional<double> keyboard_brightness_for_account =
      known_user.FindDoublePath(account_id, prefs::kKeyboardBrightnessPercent);
  if (!*has_sensor_) {
    // Only restore brightness percent if device does not have sensor.
    if (keyboard_brightness_for_account.has_value()) {
      HandleSetKeyboardBrightness(
          *keyboard_brightness_for_account,
          /*gradual=*/true,
          KeyboardBrightnessChangeSource::kRestoredFromUserPref);
    }
    return;
  }
  // If device has a sensor, restore both ambient light sensor and brightness
  // percent.
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
    if (ShouldRestoreKeyboardAmbientLightSensor(account_id, known_user)) {
      HandleSetKeyboardAmbientLightSensorEnabled(
          keyboard_ambient_light_sensor_enabled_for_account,
          KeyboardAmbientLightSensorEnabledChangeSource::kRestoredFromUserPref);
    }
    if (!keyboard_ambient_light_sensor_enabled_for_account) {
      // If the keyboard ambient light sensor is disabled, restore the user's
      // preferred keyboard brightness level.
      if (keyboard_brightness_for_account.has_value()) {
        HandleSetKeyboardBrightness(
            *keyboard_brightness_for_account,
            /*gradual=*/true,
            KeyboardBrightnessChangeSource::kRestoredFromUserPref);
      }
    }
  }

  // Record the keyboard ambient light sensor status at login.
  if (*has_sensor_ &&
      !has_keyboard_ambient_light_sensor_status_been_recorded_) {
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
    has_keyboard_backlight_ = has_keyboard_backlight;
    MaybeRestoreKeyboardBrightnessSettings();
    base::UmaHistogramBoolean("ChromeOS.Keyboard.HasBacklight",
                              *has_keyboard_backlight);
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
  has_sensor_ = has_sensor;
  MaybeRestoreKeyboardBrightnessSettings();
  base::UmaHistogramBoolean("ChromeOS.Keyboard.HasAmbientLightSensor",
                            *has_sensor);
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
  known_user.SetPath(*active_account_id_, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(*keyboard_brightness));
}

void KeyboardBrightnessController::RecordHistogramForBrightnessAction(
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
  if (time_since_last_session_change >= base::Hours(1)) {
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
      base::StrCat({"ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.",
                    is_on_login_screen ? "OnLoginScreen" : "AfterLogin", ".",
                    GetBrightnessActionName(brightness_action), "Brightness.",
                    IsChargerConnected() ? "Charger" : "Battery", "Power"}),
      time_since_last_session_change);
}

}  // namespace ash
