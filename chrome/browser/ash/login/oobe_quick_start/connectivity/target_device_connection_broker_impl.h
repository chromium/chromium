// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::quick_start {

class FastPairAdvertiser;

class TargetDeviceConnectionBrokerImpl
    : public TargetDeviceConnectionBroker,
      public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  using FeatureSupportStatus =
      TargetDeviceConnectionBroker::FeatureSupportStatus;
  using ResultCallback = TargetDeviceConnectionBroker::ResultCallback;

  // Thin wrapper around BluetoothAdapterFactory to allow mocking GetAdapter()
  // for unit tests.
  class BluetoothAdapterFactoryWrapper {
   public:
    static void GetAdapter(
        device::BluetoothAdapterFactory::AdapterCallback callback);

    static void set_bluetooth_adapter_factory_wrapper_for_testing(
        BluetoothAdapterFactoryWrapper* wrapper) {
      bluetooth_adapter_factory_wrapper_for_testing_ = wrapper;
    }

   private:
    virtual void GetAdapterImpl(
        device::BluetoothAdapterFactory::AdapterCallback callback) = 0;

    static BluetoothAdapterFactoryWrapper*
        bluetooth_adapter_factory_wrapper_for_testing_;
  };

  TargetDeviceConnectionBrokerImpl(RandomSessionId session_id,
                                   base::WeakPtr<NearbyConnectionsManager>);
  TargetDeviceConnectionBrokerImpl(TargetDeviceConnectionBrokerImpl&) = delete;
  TargetDeviceConnectionBrokerImpl& operator=(
      TargetDeviceConnectionBrokerImpl&) = delete;
  ~TargetDeviceConnectionBrokerImpl() override;

  // TargetDeviceConnectionBroker:
  FeatureSupportStatus GetFeatureSupportStatus() const override;
  void StartAdvertising(ConnectionLifecycleListener* listener,
                        ResultCallback on_start_advertising_callback) override;
  void StopAdvertising(base::OnceClosure on_stop_advertising_callback) override;

 private:
  // Used to access the |random_session_id_| in tests, and to allow testing
  // |GenerateEndpointInfo()| directly.
  friend class TargetDeviceConnectionBrokerImplTest;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnection(const std::string& endpoint_id,
                            const std::vector<uint8_t>& endpoint_info,
                            NearbyConnection* connection) override;

  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void StartFastPairAdvertising(ResultCallback callback);
  void OnStartFastPairAdvertisingSuccess(ResultCallback callback);
  void OnStartFastPairAdvertisingError(ResultCallback callback);
  void OnStopFastPairAdvertising(base::OnceClosure callback);

  // The EndpointInfo is the set of bytes that SmartSetup on Android expects to
  // be in the Nearby Connections advertisement.
  std::vector<uint8_t> GenerateEndpointInfo();

  void StartNearbyConnectionsAdvertising(ResultCallback callback);
  void StopNearbyConnectionsAdvertising(base::OnceClosure callback);
  void OnStartNearbyConnectionsAdvertising(
      ResultCallback callback,
      NearbyConnectionsManager::ConnectionsStatus status);
  void OnStopNearbyConnectionsAdvertising(
      base::OnceClosure callback,
      NearbyConnectionsManager::ConnectionsStatus status);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::OnceClosure deferred_start_advertising_callback_;

  std::unique_ptr<FastPairAdvertiser> fast_pair_advertiser_;
  RandomSessionId random_session_id_;

  base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager_;

  base::WeakPtrFactory<TargetDeviceConnectionBrokerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
