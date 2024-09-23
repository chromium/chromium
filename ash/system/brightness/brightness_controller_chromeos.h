// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

class AccountId;
class PrefService;

namespace ash {

class SessionControllerImpl;

namespace system {

enum class BrightnessAction {
  kDecreaseBrightness = 0,
  kIncreaseBrightness = 1,
  kSetBrightness = 2,
};

// A class which controls brightness when F6, F7 or a multimedia key for
// brightness is pressed.
class ASH_EXPORT BrightnessControllerChromeos
    : public BrightnessControlDelegate,
      public chromeos::PowerManagerClient::Observer,
      public LoginDataDispatcher::Observer,
      public SessionObserver {
 public:
  BrightnessControllerChromeos(PrefService* local_state,
                               SessionControllerImpl* session_controller);

  BrightnessControllerChromeos(const BrightnessControllerChromeos&) = delete;
  BrightnessControllerChromeos& operator=(const BrightnessControllerChromeos&) =
      delete;

  ~BrightnessControllerChromeos() override;

  // Registers user profile prefs with the specified registry.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Overridden from ash::BrightnessControlDelegate:
  void HandleBrightnessDown() override;
  void HandleBrightnessUp() override;
  void SetBrightnessPercent(double percent,
                            bool gradual,
                            BrightnessChangeSource source) override;
  void GetBrightnessPercent(
      base::OnceCallback<void(std::optional<double>)> callback) override;
  void SetAmbientLightSensorEnabled(
      bool enabled,
      AmbientLightSensorEnabledChangeSource source) override;
  void GetAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override;
  void HasAmbientLightSensor(
      base::OnceCallback<void(std::optional<bool>)> callback) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void AmbientLightSensorEnabledChanged(
      const power_manager::AmbientLightSensorChange& change) override;

  // LoginDataDispatcher::Observer:
  void OnFocusPod(const AccountId& account_id) override;

 private:
  void RecordHistogramForBrightnessAction(BrightnessAction brightness_action);
  void OnGetBrightnessAfterLogin(std::optional<double> brightness_percent);
  void OnGetHasAmbientLightSensor(std::optional<bool> has_sensor);
  void RestoreBrightnessSettings(const AccountId& account_id);
  void MaybeRestoreBrightnessSettings();
  void RestoreBrightnessSettingsOnFirstLogin();
  bool IsInitialBrightnessSetByPolicy();

  raw_ptr<PrefService> local_state_;
  raw_ptr<SessionControllerImpl> session_controller_;
  raw_ptr<PrefService> active_pref_service_;

  // The current AccountId, used to set and retrieve prefs. Expected to be
  // nullopt on the login screen, but will be set on login.
  std::optional<AccountId> active_account_id_;

  // Timestamp of the last session change, e.g. when going from the login screen
  // to the desktop, or from startup to the login screen.
  base::TimeTicks last_session_change_time_;

  // Used for metrics recording. True if and only if a brightness adjustment has
  // occurred.
  bool has_brightness_been_adjusted_ = false;

  // True if the ambient light sensor value has already been restored for a
  // user's first login.
  bool has_ambient_light_sensor_been_restored_for_new_user_ = false;

  // True if the ambient light sensor status has already been recorded at login
  // screen, it is used to ensures the status is recorded only once per boot.
  bool has_ambient_light_sensor_status_been_recorded_ = false;

  // True if device has an ambient light sensor.
  std::optional<bool> has_sensor_ = false;

  // This PrefChangeRegistrar is used to check when the synced profile pref for
  // the ambient light sensor value has finished syncing.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<BrightnessControllerChromeos> weak_ptr_factory_{this};
};
}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
