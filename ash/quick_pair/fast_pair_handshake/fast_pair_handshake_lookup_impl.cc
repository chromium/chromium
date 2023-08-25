// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup_impl.h"

#include <memory>

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl.h"
#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// Create function override which can be set by tests.
absl::optional<FastPairHandshakeLookup::CreateFunction> g_test_create_function =
    absl::nullopt;

// static
FastPairHandshakeLookupImpl* FastPairHandshakeLookupImpl::GetImplInstance() {
  return base::Singleton<FastPairHandshakeLookupImpl>::get();
}

// static
void FastPairHandshakeLookupImpl::SetImplCreateFunctionForTesting(
    CreateFunction create_function) {
  g_test_create_function = std::move(create_function);
}

FastPairHandshakeLookupImpl::FastPairHandshakeLookupImpl() {}
FastPairHandshakeLookupImpl::~FastPairHandshakeLookupImpl() {}

FastPairHandshake* FastPairHandshakeLookupImpl::Get(
    scoped_refptr<Device> device) {
  auto it = fast_pair_handshakes_.find(device);
  return it != fast_pair_handshakes_.end() ? it->second.get() : nullptr;
}

FastPairHandshake* FastPairHandshakeLookupImpl::Get(
    const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      return pair.second.get();
    }
  }

  return nullptr;
}

bool FastPairHandshakeLookupImpl::Erase(scoped_refptr<Device> device) {
  return fast_pair_handshakes_.erase(device) == 1;
}

bool FastPairHandshakeLookupImpl::Erase(const std::string& address) {
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->classic_address() == address ||
        pair.first->ble_address() == address) {
      fast_pair_handshakes_.erase(pair);
      return true;
    }
  }

  return false;
}

void FastPairHandshakeLookupImpl::Clear() {
  fast_pair_handshakes_.clear();
}

void FastPairHandshakeLookupImpl::Create(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete) {
  auto it = fast_pair_handshakes_.emplace(
      device, g_test_create_function.has_value()
                  ? g_test_create_function->Run(device, std::move(on_complete))
                  : std::make_unique<FastPairHandshakeImpl>(
                        std::move(adapter), device, std::move(on_complete)));

  DCHECK(it.second) << "An existing item shouldn't exist.";
}

}  // namespace quick_pair
}  // namespace ash
