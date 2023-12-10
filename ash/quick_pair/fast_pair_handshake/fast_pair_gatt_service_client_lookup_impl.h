// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_IMPL_H_

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup.h"

namespace ash {
namespace quick_pair {

// This class creates, deletes and exposes FastPairGattServiceClient instances.
class FastPairGattServiceClientLookupImpl
    : public FastPairGattServiceClientLookup {
 public:
  static FastPairGattServiceClientLookupImpl* GetImplInstance();

  FastPairGattServiceClientLookupImpl(
      const FastPairGattServiceClientLookupImpl&) = delete;
  FastPairGattServiceClientLookupImpl& operator=(
      const FastPairGattServiceClientLookupImpl&) = delete;

  // FastPairGattServiceClientLookup:
  FastPairGattServiceClient* Get(device::BluetoothDevice* device) override;
  bool Erase(device::BluetoothDevice* device) override;
  void Clear() override;
  void Create(scoped_refptr<device::BluetoothAdapter> adapter,
              device::BluetoothDevice* device,
              OnCompleteCallback on_complete) override;
  void InsertFakeForTesting(
      device::BluetoothDevice* device,
      std::unique_ptr<FastPairGattServiceClient> client) override;

 protected:
  FastPairGattServiceClientLookupImpl();
  virtual ~FastPairGattServiceClientLookupImpl();

 private:
  friend struct base::DefaultSingletonTraits<
      FastPairGattServiceClientLookupImpl>;

  base::flat_map<device::BluetoothDevice*,
                 std::unique_ptr<FastPairGattServiceClient>>
      fast_pair_gatt_service_clients_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_LOOKUP_IMPL_H_
