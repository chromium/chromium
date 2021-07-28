// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include "ash/quick_pair/common/pair_failure.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetoothDevice;
class BluetoothGattConnection;
class BluetoothRemoteGattService;

}  // namespace device

namespace ash {
namespace quick_pair {

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClient : public device::BluetoothAdapter::Observer {
 public:
  FastPairGattServiceClient(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(absl::optional<PairFailure>)>
          on_initialized_callback);
  ~FastPairGattServiceClient() override;
  FastPairGattServiceClient(const FastPairGattServiceClient&) = delete;
  FastPairGattServiceClient& operator=(const FastPairGattServiceClient&) =
      delete;

  device::BluetoothRemoteGattService* gatt_service() { return gatt_service_; }

 private:
  // Callback from the adapter's call to create GATT connection.
  void OnGattConnection(
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Invokes the callback with the proper PairFailure and clears local state.
  void NotifyError(PairFailure failure);

  // BluetoothAdapter::Observer
  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;

  std::string device_address_;
  base::OnceCallback<void(absl::optional<PairFailure>)>
      on_initialized_callback_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;
  device::BluetoothRemoteGattService* gatt_service_ = nullptr;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<FastPairGattServiceClient> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_GATT_SERVICE_CLIENT_H_
