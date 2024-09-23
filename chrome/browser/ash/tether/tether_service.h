// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_H_
#define CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/tether/tether_component.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "device/bluetooth/bluetooth_adapter.h"

class Profile;

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace tether {

class GmsCoreNotificationsStateTracker;
class GmsCoreNotificationsStateTrackerImpl;
class NotificationPresenter;

// Service providing access to the Instant Tethering component. Provides an
// interface to start up the component as well as to retrieve metadata about
// ongoing Tether connections.
//
// This service starts up when the user logs in (or recovers from a crash) and
// is shut down when the user logs out.
class TetherService
    : public KeyedService,
      public chromeos::PowerManagerClient::Observer,
      public TetherHostFetcher::Observer,
      public device::BluetoothAdapter::Observer,
      public NetworkStateHandlerObserver,
      public TetherComponent::Observer,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  TetherService(
      Profile* profile,
      chromeos::PowerManagerClient* power_manager_client,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      session_manager::SessionManager* session_manager);
  TetherService(const TetherService&) = delete;
  TetherService& operator=(const TetherService&) = delete;
  ~TetherService() override;

  // Gets TetherService instance.
  static TetherService* Get(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Attempt to start the Tether module. Only succeeds if all conditions to
  // reach NetworkStateHandler::TechnologyState::ENABLED are reached.
  // Should only be called once a user is logged in.
  virtual void StartTetherIfPossible();

  virtual GmsCoreNotificationsStateTracker*
  GetGmsCoreNotificationsStateTracker();

 protected:
  // KeyedService:
  void Shutdown() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // TetherHostFetcher::Observer
  void OnTetherHostUpdated() override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void DevicePropertiesUpdated(const DeviceState* device) override;

  // Helper method called from NetworkStateHandlerObserver methods.
  void UpdateEnabledState();

  // TetherComponent::Observer:
  void OnShutdownComplete() override;

  // ash::device_sync::DeviceSyncClient::Observer:
  void OnReady() override;

  // ash::multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // Stop the Tether module if it is currently enabled; if it was not enabled,
  // this function is a no-op.
  virtual void StopTetherIfNecessary();

  // Whether Tether hosts are available.
  virtual bool HasSyncedTetherHosts() const;

  virtual void UpdateTetherTechnologyState();
  NetworkStateHandler::TechnologyState GetTetherTechnologyState();

  NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }

 private:
  friend class TetherServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestSuspend);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestDeviceSyncClientNotReady);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestMultiDeviceSetupClientInitiallyHasNoVerifiedHost);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestMultiDeviceSetupClientLosesVerifiedHost);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestBetterTogetherSuiteInitiallyDisabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestBetterTogetherSuiteBecomesDisabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestGet_PrimaryUser_FeatureFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiAndMultiDeviceSetupFlagsEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestNoTetherHosts);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestProhibitedByPolicy);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestIsBluetoothPowered);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestCellularIsUnavailable);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestCellularIsAvailable_InstantHotspotRebrandDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestCellularIsAvailable_InstantHotspotRebrandEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestDisabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestUserPrefChangesViaFeatureStateChange);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestUserPrefChangesViaTechnologyStateChange);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBluetoothNotification);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBluetoothNotPresent);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestMetricsFalsePositives);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestWifiNotPresent);

  // Reflects InstantTethering_FeatureState enum in enums.xml. Do not rearrange.
  enum TetherFeatureState {
    // Note: Value 0 was previously OTHER_OR_UNKNOWN, but this was a vague
    // description.
    SHUT_DOWN = 0,
    // Note: Value 1 was previously BLE_ADVERTISING_NOT_SUPPORTED, but this
    // value is obsolete and should no longer be used.
    // Note: Value 2 was previously SCREEN_LOCKED, but this value is obsolete
    // and should no longer be used.
    NO_AVAILABLE_HOSTS = 3,
    CELLULAR_DISABLED = 4,
    PROHIBITED = 5,
    BLUETOOTH_DISABLED = 6,
    USER_PREFERENCE_DISABLED = 7,
    ENABLED = 8,
    BLE_NOT_PRESENT = 9,
    WIFI_NOT_PRESENT = 10,
    SUSPENDED = 11,
    BETTER_TOGETHER_SUITE_DISABLED = 12,
    TETHER_FEATURE_STATE_MAX
  };

  // For debug logs.
  static std::string TetherFeatureStateToString(
      const TetherFeatureState& state);

  void GetBluetoothAdapter();
  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> adapter);

  bool IsBluetoothPresent() const;
  bool IsBluetoothPowered() const;

  bool IsWifiPresent() const;

  bool IsCellularAvailableButNotEnabled() const;

  // Whether Tether is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled.
  bool IsAllowedByPolicy() const;

  // Whether Tether is enabled.
  bool IsEnabledByPreference() const;

  TetherFeatureState GetTetherFeatureState();

  // Record to UMA Tether's current feature state.
  void RecordTetherFeatureState();

  // Attempt to record the current Tether FeatureState.
  void RecordTetherFeatureStateIfPossible();

  // Handles potential false positive metric states which may occur normally
  // during startup. In the normal case (i.e., when Tether is enabled), the
  // state transitions from OTHER_OR_UNKNOWN -> BLE_NOT_PRESENT ->
  // NO_AVAILABLE_HOSTS -> ENABLED, but we do not wish to log metrics for the
  // intermediate states (BLE_NOT_PRESENT or NO_AVAILABLE_HOSTS), since these
  // are ephemeral. Returns whether a false positive case was handled.
  bool HandleFeatureStateMetricIfUninitialized();

  void LogUserPreferenceChanged(bool is_now_enabled);

  void SetTestDoubles(
      std::unique_ptr<NotificationPresenter> notification_presenter,
      std::unique_ptr<base::OneShotTimer> timer);

  // Whether the service has been shut down.
  bool shut_down_ = false;

  // Whether the device and service have been suspended (e.g. the laptop lid
  // was closed).
  bool suspended_ = false;

  bool is_adapter_being_fetched_ = false;

  multidevice_setup::mojom::HostStatus host_status_ =
      multidevice_setup::mojom::HostStatus::kNoEligibleHosts;

  // The first report of TetherFeatureState::BLE_NOT_PRESENT is usually
  // incorrect and hence is a false positive. This property tracks if the first
  // report has been hit yet.
  bool ble_not_present_false_positive_encountered_ = false;

  // The first report of TetherFeatureState::NO_AVAILABLE_HOSTS may be incorrect
  // and hence a false positive. This property tracks if the first report has
  // been hit yet.
  bool no_available_hosts_false_positive_encountered_ = false;

  // The TetherFeatureState obtained the last time that
  // GetTetherTechnologyState() was called. Used only for logging purposes.
  TetherFeatureState previous_feature_state_ =
      TetherFeatureState::TETHER_FEATURE_STATE_MAX;

  raw_ptr<Profile> profile_;
  raw_ptr<chromeos::PowerManagerClient> power_manager_client_;
  raw_ptr<device_sync::DeviceSyncClient, DanglingUntriaged> device_sync_client_;
  raw_ptr<secure_channel::SecureChannelClient> secure_channel_client_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient, DanglingUntriaged>
      multidevice_setup_client_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<NotificationPresenter> notification_presenter_;
  std::unique_ptr<GmsCoreNotificationsStateTrackerImpl>
      gms_core_notifications_state_tracker_;
  std::unique_ptr<TetherHostFetcher> tether_host_fetcher_;
  std::unique_ptr<TetherComponent> tether_component_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<TetherService> weak_ptr_factory_{this};
};

}  // namespace tether
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TETHER_TETHER_SERVICE_H_
