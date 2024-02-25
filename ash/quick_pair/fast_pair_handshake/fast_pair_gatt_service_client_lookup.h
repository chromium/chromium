// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

// This class creates, deletes and exposes FastPairGattServiceClient instances.
class FastPairGattServiceClientLookup {
 public:
  using OnCompleteCallback =
      base::OnceCallback<void(std::optional<PairFailure>)>;

  static FastPairGattServiceClientLookup* GetInstance();

  // Get an existing instance for |device|.
  virtual FastPairGattServiceClient* Get(device::BluetoothDevice* device) = 0;

  // Erases the FastPairGattServiceClient instance for |device| if exists.
  virtual bool Erase(device::BluetoothDevice* device) = 0;

  // Deletes all existing FastPairGattServiceClient instances.
  virtual void Clear() = 0;

  // Creates and returns a new instance for |device| if no instance already
  // exists. Returns the existing instance if there is one.
  virtual void Create(scoped_refptr<device::BluetoothAdapter> adapter,
                      device::BluetoothDevice* device,
                      OnCompleteCallback on_complete) = 0;

  // TODO (b/310671988): Move this into a test-only fake-impl.
  virtual void InsertFakeForTesting(
      device::BluetoothDevice* device,
      std::unique_ptr<FastPairGattServiceClient> client) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_H_
