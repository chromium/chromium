// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/smart_lock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_auth_attempt.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_metrics.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/keyed_service/core/keyed_service.h"

class AccountId;
class Profile;

namespace user_manager {
class User;
}  // namespace user_manager

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace proximity_auth {
class ProximityAuthProfilePrefManager;
class ProximityAuthSystem;
}  // namespace proximity_auth

namespace ash {

class SmartLockNotificationController;
class SmartLockFeatureUsageMetrics;
enum class SmartLockState;

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

class SmartLockService
    : public KeyedService,
      public proximity_auth::ScreenlockBridge::Observer,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  // Gets SmartLockService instance.
  static SmartLockService* Get(Profile* profile);

  // Gets SmartLockService instance associated with a user if the user is
  // logged in and their profile is initialized.
  static SmartLockService* GetForUser(const user_manager::User& user);

  // Registers Smart Lock profile prefs. Note that these pref names refer to
  // Smart Lock using the deprecated "Easy Unlock" name.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  SmartLockService(
      Profile* profile,
      secure_channel::SecureChannelClient* secure_channel_client,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  // Constructor for tests.
  SmartLockService(
      Profile* profile,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<SmartLockNotificationController> notification_controller,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  SmartLockService(const SmartLockService&) = delete;
  SmartLockService& operator=(const SmartLockService&) = delete;
  ~SmartLockService() override;

  // Starts an auth attempt for the user associated with the service. Returns
  // true if no other attempt is in progress and the attempt request can be
  // processed.
  bool AttemptAuth(const AccountId& account_id);

  // Finalizes the previously started auth attempt for smart lock.
  void FinalizeUnlock(bool success);

  // Returns the user currently associated with the service.
  AccountId GetAccountId() const;

  // To be called when SmartLockService is "warming up", for example, on screen
  // lock, after suspend, etc. During a period like this, not all sub-systems
  // are fully initialized, particularly UnlockManager and the Bluetooth stack,
  // so to avoid UI jank, callers can use this method to fill in the UI with an
  // approximation of what the UI will look like in <1 second. The resulting
  // initial state will be one of two possibilities:
  //   * SmartLockState::kConnectingToPhone: if the feature is allowed, enabled,
  //     and has kicked off a scan/connection.
  //   * SmartLockState::kDisabled: if any values above can't be confirmed.
  SmartLockState GetInitialSmartLockState() const;

  // The last value emitted to the SmartLock.GetRemoteStatus.Unlock(.Failure)
  // metrics. Helps to understand whether/why not Smart Lock was an available
  // choice for unlock. Returns the empty string if the ProximityAuthSystem or
  // the UnlockManager is uninitialized.
  std::string GetLastRemoteStatusUnlockForLogging();

  // Retrieves the remote device list stored for the account in
  // |proximity_auth_system_|.
  const multidevice::RemoteDeviceRefList GetRemoteDevicesForTesting() const;

  // Sets the service up and schedules service initialization.
  void Initialize();

  // Whether Smart Lock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled. Virtual to allow override for testing.
  virtual bool IsAllowed() const;

  // Whether Smart Lock is currently enabled for this user. Virtual to allow
  // override for testing.
  virtual bool IsEnabled() const;

  ChromeProximityAuthClient* proximity_auth_client() {
    return &proximity_auth_client_;
  }

  // Updates the user pod on the lock screen for the user associated with
  // the service to reflect the provided Smart Lock state.
  void UpdateSmartLockState(SmartLockState state);

 private:
  friend class SmartLockServiceTest;

  // KeyedService:
  void Shutdown() override;

  // proximity_auth::ScreenlockBridge::Observer:
  void OnScreenDidLock() override;
  void OnScreenDidUnlock() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // Returns the authentication event for a recent password unlock,
  // according to the current state of the service.
  SmartLockAuthEvent GetPasswordAuthEvent() const;

  // Returns the authentication event for a recent password unlock,
  // according to the current state of the service.
  SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
  GetSmartUnlockPasswordAuthEvent() const;

  multidevice::RemoteDeviceRefList GetUnlockKeys();

  // Determines whether failure to unlock with phone should be handled as an
  // authentication failure.
  bool IsSmartLockStateValidOnRemoteAuthFailure() const;

  // Loads the RemoteDevice instances that will be supplied to
  // ProximityAuthSystem.
  void LoadRemoteDevices();

  void NotifySmartLockAuthResult(bool success);

  // Called when the system resumes from a suspended state.
  void OnSuspendDone();

  // Called when the user enters password before smart lock succeeds or fails
  // definitively.
  void OnUserEnteredPassword();

  // Updates the service to state for handling system suspend.
  void PrepareForSuspend();

  // Exposes the profile to which the service is attached to subclasses.
  Profile* profile() const { return profile_; }

  void RecordAuthResult(
      std::optional<SmartLockMetricsRecorder::SmartLockAuthResultFailureReason>
          failure_reason);

  // Resets the Smart Lock state set by this service.
  void ResetSmartLockState();

  // Called by subclasses when remote devices allowed to unlock the screen
  // are loaded for `account_id`.
  void SetProximityAuthDevices(
      const AccountId& account_id,
      const multidevice::RemoteDeviceRefList& remote_devices,
      std::optional<multidevice::RemoteDeviceRef> local_device);

  void set_will_authenticate_using_smart_lock(
      bool will_authenticate_using_smart_lock) {
    will_authenticate_using_smart_lock_ = will_authenticate_using_smart_lock;
  }

  void ShowChromebookAddedNotification();

  // Fill in the UI with the state returned by GetInitialSmartLockState().
  void ShowInitialSmartLockState();

  void ShowNotificationIfNewDevicePresent(
      const std::set<std::string>& public_keys_before_sync,
      const std::set<std::string>& public_keys_after_sync);

  // Called when ready to begin recording Smart Lock feature usage
  // within Standard Feature Usage Logging (SFUL) framework.
  void StartFeatureUsageMetrics();

  // Called when ready to stop recording Smart Lock feature usage
  // within SFUL framework.
  void StopFeatureUsageMetrics();

  // Checks whether Smart Lock should be running and updates app state.
  void UpdateAppState();

  void UseLoadedRemoteDevices(
      const multidevice::RemoteDeviceRefList& remote_devices);

  bool will_authenticate_using_smart_lock() const {
    return will_authenticate_using_smart_lock_;
  }

  // True if the user just authenticated using Smart Lock. Reset once
  // the screen unlocks. Used to distinguish Smart Lock-powered
  // unlocks from password-based unlocks for metrics.
  bool will_authenticate_using_smart_lock_ = false;

  const raw_ptr<Profile> profile_;
  raw_ptr<secure_channel::SecureChannelClient> secure_channel_client_;

  ChromeProximityAuthClient proximity_auth_client_;

  std::optional<SmartLockState> smart_lock_state_;

  // The handler for the current auth attempt. Set iff an auth attempt is in
  // progress.
  std::unique_ptr<SmartLockAuthAttempt> auth_attempt_;

  // Handles connecting, authenticating, and updating the UI on the lock
  // screen. After a `RemoteDeviceRef` instance is provided, this object will
  // handle the rest.
  std::unique_ptr<proximity_auth::ProximityAuthSystem> proximity_auth_system_;

  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  std::unique_ptr<PowerMonitor> power_monitor_;

  // Whether the service has been shut down.
  bool shut_down_;

  // The timestamp for the most recent time when the lock screen was shown. The
  // lock screen is typically shown when the user awakens their computer from
  // sleep -- e.g. by opening the lid -- but can also be shown if the screen is
  // locked but the computer does not go to sleep.
  base::TimeTicks lock_screen_last_shown_timestamp_;

  // Manager responsible for handling the prefs used by proximity_auth classes.
  std::unique_ptr<proximity_auth::ProximityAuthProfilePrefManager>
      pref_manager_;

  // If a new RemoteDevice was synced while the screen is locked, we defer
  // loading the RemoteDevice until the screen is unlocked. For security,
  // this deferment prevents the lock screen from being changed by a network
  // event.
  bool deferring_device_load_ = false;

  // Responsible for showing all the notifications used for Smart Lock.
  std::unique_ptr<SmartLockNotificationController> notification_controller_;

  // Used to fetch local device and remote device data.
  raw_ptr<device_sync::DeviceSyncClient, DanglingUntriaged> device_sync_client_;

  // Used to determine the FeatureState of Smart Lock.
  raw_ptr<multidevice_setup::MultiDeviceSetupClient, DanglingUntriaged>
      multidevice_setup_client_;

  // Tracks Smart Lock feature usage for the Standard Feature Usage Logging
  // (SFUL) framework.
  std::unique_ptr<SmartLockFeatureUsageMetrics> feature_usage_metrics_;

  // Stores the unlock keys for Smart Lock before the current device sync, so we
  // can compare it to the unlock keys after syncing.
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_before_sync_;
  multidevice::RemoteDeviceRefList remote_device_unlock_keys_before_sync_;

  // True if the pairing changed notification was shown, so that the next time
  // the Chromebook is unlocked, we can show the subsequent 'pairing applied'
  // notification.
  bool shown_pairing_changed_notification_ = false;

  base::WeakPtrFactory<SmartLockService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_SERVICE_H_
