// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"

#include <memory>

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// static
FastPairGattServiceClientLookupImpl*
FastPairGattServiceClientLookupImpl::GetImplInstance() {
  return base::Singleton<FastPairGattServiceClientLookupImpl>::get();
}

FastPairGattServiceClientLookupImpl::FastPairGattServiceClientLookupImpl() {}
FastPairGattServiceClientLookupImpl::~FastPairGattServiceClientLookupImpl() {}

FastPairGattServiceClient* FastPairGattServiceClientLookupImpl::Get(
    device::BluetoothDevice* device) {
  auto it = fast_pair_gatt_service_clients_.find(device);
  return it != fast_pair_gatt_service_clients_.end() ? it->second.get()
                                                     : nullptr;
}

bool FastPairGattServiceClientLookupImpl::Erase(
    device::BluetoothDevice* device) {
  return fast_pair_gatt_service_clients_.erase(device) == 1;
}

void FastPairGattServiceClientLookupImpl::Clear() {
  fast_pair_gatt_service_clients_.clear();
}

// TODO (b/308825200): Replace with GetOrCreate function so client
// is always returned in ready state.
void FastPairGattServiceClientLookupImpl::Create(
    scoped_refptr<device::BluetoothAdapter> adapter,
    device::BluetoothDevice* device,
    OnCompleteCallback on_complete) {
  auto fast_pair_gatt_service_client =
      FastPairGattServiceClientImpl::Factory::Create(device, adapter,
                                                     std::move(on_complete));

  auto it = fast_pair_gatt_service_clients_.emplace(
      device, std::move(fast_pair_gatt_service_client));

  DCHECK(it.second) << "An existing item shouldn't exist.";
}

void FastPairGattServiceClientLookupImpl::InsertFakeForTesting(
    device::BluetoothDevice* device,
    std::unique_ptr<FastPairGattServiceClient> client) {
  auto it = fast_pair_gatt_service_clients_.emplace(device, std::move(client));

  DCHECK(it.second) << "An existing item shouldn't exist.";
}

}  // namespace quick_pair
}  // namespace ash
