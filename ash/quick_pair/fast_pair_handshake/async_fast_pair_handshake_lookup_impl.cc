// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/async_fast_pair_handshake_lookup_impl.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl_new.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "device/bluetooth/bluetooth_adapter.h"

constexpr int kMaxNumHandshakeAttempts = 3;

namespace ash::quick_pair {

// static
AsyncFastPairHandshakeLookupImpl*
AsyncFastPairHandshakeLookupImpl::GetAsyncInstance() {
  return base::Singleton<AsyncFastPairHandshakeLookupImpl>::get();
}

AsyncFastPairHandshakeLookupImpl::AsyncFastPairHandshakeLookupImpl() = default;
AsyncFastPairHandshakeLookupImpl::~AsyncFastPairHandshakeLookupImpl() {}

FastPairHandshake* AsyncFastPairHandshakeLookupImpl::Get(
    scoped_refptr<Device> device) {
  auto it = fast_pair_handshakes_.find(device);
  return it != fast_pair_handshakes_.end() ? it->second.get() : nullptr;
}

FastPairHandshake* AsyncFastPairHandshakeLookupImpl::Get(
    const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      return pair.second.get();
    }
  }

  return nullptr;
}

bool AsyncFastPairHandshakeLookupImpl::Erase(scoped_refptr<Device> device) {
  fast_pair_handshake_attempt_counts_.erase(device);
  return fast_pair_handshakes_.erase(device) == 1;
}

bool AsyncFastPairHandshakeLookupImpl::Erase(const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      fast_pair_handshake_attempt_counts_.erase(pair.first);
      fast_pair_handshakes_.erase(pair);
      return true;
    }
  }

  return false;
}

void AsyncFastPairHandshakeLookupImpl::Clear() {
  fast_pair_handshake_attempt_counts_.clear();
  fast_pair_handshakes_.clear();
}

void AsyncFastPairHandshakeLookupImpl::Create(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete) {
  std::unique_ptr<FastPairHandshakeImplNew> handshake_to_emplace =
      std::make_unique<FastPairHandshakeImplNew>(std::move(adapter), device);

  CHECK(!fast_pair_handshakes_.contains(device))
      << "An existing item shouldn't exist";

  fast_pair_handshakes_.emplace(device, std::move(handshake_to_emplace));

  AttemptHandshakeWithRetries(device, std::move(on_complete), std::nullopt);
}

void AsyncFastPairHandshakeLookupImpl::AttemptHandshakeWithRetries(
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete_callback,
    std::optional<PairFailure> failure) {
  auto* handshake = Get(device);
  CHECK(handshake);

  // Reset the handshake in case this is a retry attempt we want to make sure we
  // are always starting from clean slate.
  handshake->Reset();
  if (fast_pair_handshake_attempt_counts_[device] < kMaxNumHandshakeAttempts) {
    fast_pair_handshake_attempt_counts_[device]++;

    // The callback will either be used during a retry as a failure or in
    // OnHandshakeComplete() but never both for the same callback so we split it
    // here to accommodate both situations.
    auto split_callback =
        base::SplitOnceCallback(std::move(on_complete_callback));
    handshake->SetUpHandshake(
        base::BindOnce(
            &AsyncFastPairHandshakeLookupImpl::AttemptHandshakeWithRetries,
            weak_ptr_factory_.GetWeakPtr(), device,
            std::move(split_callback.first)),
        base::BindOnce(&AsyncFastPairHandshakeLookupImpl::OnHandshakeComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(split_callback.second)));
  } else {
    Erase(device);
    std::move(on_complete_callback).Run(device, failure);
  }
}

void AsyncFastPairHandshakeLookupImpl::OnHandshakeComplete(
    OnCompleteCallback on_complete_callback,
    scoped_refptr<Device> device) {
  RecordHandshakeAttemptCount(fast_pair_handshake_attempt_counts_[device]);

  // Reset |num_handshake_attempts_| so if the handshake is lost during pairing,
  // we will attempt to create it 3 more times. This should be an extremely rare
  // situation, such as handshake happening directly before the device rotates
  // ble addresses.
  fast_pair_handshake_attempt_counts_.erase(device);
  std::move(on_complete_callback).Run(device, std::nullopt);
}

}  // namespace ash::quick_pair
