// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake_lookup.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl_new.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::quick_pair {

// static
FakeFastPairHandshakeLookup* FakeFastPairHandshakeLookup::GetFakeInstance() {
  return base::Singleton<FakeFastPairHandshakeLookup>::get();
}

FakeFastPairHandshakeLookup::FakeFastPairHandshakeLookup() = default;
FakeFastPairHandshakeLookup::~FakeFastPairHandshakeLookup() {}

FastPairHandshake* FakeFastPairHandshakeLookup::Get(
    scoped_refptr<Device> device) {
  auto it = fast_pair_handshakes_.find(device);
  return it != fast_pair_handshakes_.end() ? it->second.get() : nullptr;
}

FastPairHandshake* FakeFastPairHandshakeLookup::Get(
    const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      return pair.second.get();
    }
  }

  return nullptr;
}

bool FakeFastPairHandshakeLookup::Erase(scoped_refptr<Device> device) {
  return fast_pair_handshakes_.erase(device) == 1;
}

bool FakeFastPairHandshakeLookup::Erase(const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      fast_pair_handshakes_.erase(pair);
      return true;
    }
  }

  return false;
}

void FakeFastPairHandshakeLookup::Clear() {
  fast_pair_handshakes_.clear();
}

void FakeFastPairHandshakeLookup::Create(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete) {
  on_complete_callback_ = std::move(on_complete);
  std::unique_ptr<FastPairHandshakeImplNew> handshake_to_emplace =
      std::make_unique<FastPairHandshakeImplNew>(std::move(adapter), device);

  CHECK(!fast_pair_handshakes_.contains(device))
      << "An existing item shouldn't exist";
  handshake_to_emplace->set_completed_successfully();

  fast_pair_handshakes_.emplace(device, std::move(handshake_to_emplace));
}

void FakeFastPairHandshakeLookup::InvokeCallbackForTesting(
    scoped_refptr<Device> device,
    std::optional<PairFailure> failure) {
  std::move(on_complete_callback_).Run(device, failure);
}

void FakeFastPairHandshakeLookup::CreateForTesting(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete,
    std::unique_ptr<FastPairGattServiceClient> gatt_service_client,
    std::unique_ptr<FastPairDataEncryptor> data_encryptor) {
  on_complete_callback_ = std::move(on_complete);

  std::unique_ptr<FastPairHandshake> handshake_to_emplace =
      std::make_unique<FakeFastPairHandshake>(
          std::move(adapter), device, base::DoNothing(),
          std::move(data_encryptor), std::move(gatt_service_client));
  handshake_to_emplace->set_completed_successfully();
  fast_pair_handshakes_.emplace(device, std::move(handshake_to_emplace));
  return;
}

}  // namespace ash::quick_pair
