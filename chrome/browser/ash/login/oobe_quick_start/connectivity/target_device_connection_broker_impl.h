// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::quick_start {

class FastPairAdvertiser;

class TargetDeviceConnectionBrokerImpl : public TargetDeviceConnectionBroker {
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

  explicit TargetDeviceConnectionBrokerImpl(RandomSessionId session_id);
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
  friend class TargetDeviceConnectionBrokerImplTest;

  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartFastPairAdvertisingError(ResultCallback callback);
  void OnStopFastPairAdvertising(base::OnceClosure callback);

  // The EndpointInfo is the set of bytes that SmartSetup on Android expects to
  // be in the Nearby Connections advertisement.
  std::vector<uint8_t> GenerateEndpointInfo();

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::OnceClosure deferred_start_advertising_callback_;

  std::unique_ptr<FastPairAdvertiser> fast_pair_advertiser_;
  RandomSessionId random_session_id_;

  base::WeakPtrFactory<TargetDeviceConnectionBrokerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
