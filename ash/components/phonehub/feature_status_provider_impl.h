// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_

#include "ash/components/phonehub/feature_status_provider.h"
#include "ash/services/device_sync/public/cpp/device_sync_client.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace phonehub {

// FeatureStatusProvider implementation which utilizes DeviceSyncClient,
// MultiDeviceSetupClient and BluetoothAdapter to determine the current status.
class FeatureStatusProviderImpl
    : public FeatureStatusProvider,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public device::BluetoothAdapter::Observer,
      public secure_channel::ConnectionManager::Observer,
      public session_manager::SessionManagerObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  FeatureStatusProviderImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager,
      session_manager::SessionManager* session_manager,
      PowerManagerClient* power_manager_client);
  ~FeatureStatusProviderImpl() override;

 private:
  friend class FeatureStatusProviderImplTest;

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;
  void OnNewDevicesSynced() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void OnBluetoothAdapterReceived(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void UpdateStatus();
  FeatureStatus ComputeStatus();
  bool IsBluetoothOn() const;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  void RecordFeatureStatusOnLogin();

  device_sync::DeviceSyncClient* device_sync_client_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  secure_channel::ConnectionManager* connection_manager_;
  session_manager::SessionManager* session_manager_;
  PowerManagerClient* power_manager_client_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  absl::optional<FeatureStatus> status_;
  bool is_login_status_metric_recorded_ = false;
  bool is_suspended_ = false;

  base::WeakPtrFactory<FeatureStatusProviderImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_
