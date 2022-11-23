// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class ListValue;
}  // namespace base

namespace proximity_auth {
class ProximityAuthProfilePrefManager;
}  // namespace proximity_auth

class Profile;

namespace ash {

class EasyUnlockNotificationController;
class SmartLockFeatureUsageMetrics;

namespace secure_channel {
class SecureChannelClient;
}

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular
    : public EasyUnlockService,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  EasyUnlockServiceRegular(
      Profile* profile,
      secure_channel::SecureChannelClient* secure_channel_client,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  // Constructor for tests.
  EasyUnlockServiceRegular(
      Profile* profile,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<EasyUnlockNotificationController> notification_controller,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  EasyUnlockServiceRegular(const EasyUnlockServiceRegular&) = delete;
  EasyUnlockServiceRegular& operator=(const EasyUnlockServiceRegular&) = delete;

  ~EasyUnlockServiceRegular() override;

 private:
  friend class EasyUnlockServiceRegularTest;

  // Loads the RemoteDevice instances that will be supplied to
  // ProximityAuthSystem.
  void LoadRemoteDevices();

  void UseLoadedRemoteDevices(
      const multidevice::RemoteDeviceRefList& remote_devices);

  // Persists Smart Lock host and local device to prefs, and then informs
  // the base class to potentially update Smart Lock host and local device
  // stored in the TPM.
  void SetStoredRemoteDevices(const base::Value::List& devices);

  // EasyUnlockService implementation:
  proximity_auth::ProximityAuthPrefManager* GetProximityAuthPrefManager()
      override;
  EasyUnlockService::Type GetType() const override;
  AccountId GetAccountId() const override;
  const base::Value::List* GetRemoteDevices() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const AccountId& account_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const AccountId& account_id) const override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() const override;
  bool IsEnabled() const override;
  bool IsChromeOSLoginEnabled() const override;

  void OnSuspendDoneInternal() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void ShowChromebookAddedNotification();

  void ShowNotificationIfNewDevicePresent(
      const std::set<std::string>& public_keys_before_sync,
      const std::set<std::string>& public_keys_after_sync);

  // Called when ready to begin recording Smart Lock feature usage
  // within Standard Feature Usage Logging (SFUL) framework.
  void StartFeatureUsageMetrics();

  // Called when ready to stop recording Smart Lock feature usage
  // within SFUL framework.
  void StopFeatureUsageMetrics();

  // EasyUnlockService:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  multidevice::RemoteDeviceRefList GetUnlockKeys();

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

  // Responsible for showing all the notifications used for EasyUnlock.
  std::unique_ptr<EasyUnlockNotificationController> notification_controller_;

  // Used to fetch local device and remote device data.
  device_sync::DeviceSyncClient* device_sync_client_;

  // Used to determine the FeatureState of Smart Lock.
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  // Tracks Smart Lock feature usage for the Standard Feature Usage Logging
  // (SFUL) framework.
  std::unique_ptr<SmartLockFeatureUsageMetrics> feature_usage_metrics_;

  // Stores the unlock keys for EasyUnlock before the current device sync, so we
  // can compare it to the unlock keys after syncing.
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_before_sync_;
  multidevice::RemoteDeviceRefList remote_device_unlock_keys_before_sync_;

  // True if the pairing changed notification was shown, so that the next time
  // the Chromebook is unlocked, we can show the subsequent 'pairing applied'
  // notification.
  bool shown_pairing_changed_notification_ = false;

  // Listens to pref changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<EasyUnlockServiceRegular> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_
