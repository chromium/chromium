// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_ADB_SIDELOADING_ALLOWANCE_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_ADB_SIDELOADING_ALLOWANCE_MODE_POLICY_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/notifications/adb_sideloading_policy_change_notification.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class OneShotTimer;
}

namespace policy {

// Enum that corresponds to the possible values of the device policy
// DeviceCrostiniArcAdbSideloadingAllowedProto.AllowanceMode.
enum class AdbSideloadingAllowanceMode {
  kNotSet = 0,
  kDisallow = 1,
  kDisallowWithPowerwash = 2,
  kAllowForAffiliatedUser = 3
};

class AdbSideloadingAllowanceModePolicyHandler
    : public chromeos::PowerManagerClient::Observer {
 public:
  // Will be invoked with the value of arc_sideloading_allowed flag in the boot
  // lockbox, or false if this flag is not set or if an error occurs while
  // checking the flag.
  using StatusResultCallback = base::OnceCallback<void(bool)>;

  // Will be invoked by AdbSideloadingAllowanceModePolicyHandler to check
  // whether ADB sideloading has ever been activated on this device and
  // therefore whether there is a need to display the notifications at all.
  using CheckSideloadingStatusCallback =
      base::RepeatingCallback<void(StatusResultCallback callback)>;

  // Defines which kind of notification should be displayed, affecting the
  // title, the message and whether or not the button is displayed
  using NotificationType = ash::AdbSideloadingPolicyChangeNotification::Type;

  AdbSideloadingAllowanceModePolicyHandler(
      ash::CrosSettings* cros_settings,
      PrefService* local_state,
      chromeos::PowerManagerClient* power_manager_client,
      ash::AdbSideloadingPolicyChangeNotification*
          adb_sideloading_policy_change_notification);

  // Not copyable or movable
  AdbSideloadingAllowanceModePolicyHandler(
      const AdbSideloadingAllowanceModePolicyHandler&) = delete;
  AdbSideloadingAllowanceModePolicyHandler& operator=(
      const AdbSideloadingAllowanceModePolicyHandler&) = delete;

  ~AdbSideloadingAllowanceModePolicyHandler() override;

  void SetCheckSideloadingStatusCallbackForTesting(
      const CheckSideloadingStatusCallback& callback);

  void SetNotificationTimerForTesting(
      std::unique_ptr<base::OneShotTimer> timer);

  void ShowAdbSideloadingPolicyChangeNotificationIfNeeded();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // chromeos::PowerManagerClient::Observer overrides.
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

 private:
  void MaybeShowNotification();

  // Check if sideloading has been activated before
  // Returns the value of kSideloadingAllowedBootAttribute
  void CheckSideloadingStatus(base::OnceCallback<void(bool)> callback);

  bool WasDisallowedNotificationShown();
  bool WasPowerwashOnNextRebootNotificationShown();

  void ClearDisallowedNotificationPref();
  void ClearPowerwashNotificationPrefs();

  void MaybeShowDisallowedNotification(bool is_sideloading_enabled);
  void MaybeShowPowerwashNotification(bool is_sideloading_enabled);
  void MaybeShowPowerwashUponRebootNotification();

  const raw_ptr<ash::CrosSettings> cros_settings_;

  const raw_ptr<PrefService> local_state_;

  std::unique_ptr<ash::AdbSideloadingPolicyChangeNotification>
      adb_sideloading_policy_change_notification_;

  std::unique_ptr<base::OneShotTimer> notification_timer_;

  base::CallbackListSubscription policy_subscription_;

  CheckSideloadingStatusCallback check_sideloading_status_callback_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observer_;

  base::WeakPtrFactory<AdbSideloadingAllowanceModePolicyHandler> weak_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_ADB_SIDELOADING_ALLOWANCE_MODE_POLICY_HANDLER_H_
