// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace ash {
namespace quick_pair {
namespace {

constexpr int kBluetoothAddressSize = 6;
FastPairRepository* g_instance = nullptr;
FastPairRepository* g_test_instance = nullptr;

}  // namespace

// static
FastPairRepository* FastPairRepository::Get() {
  if (g_test_instance) {
    return g_test_instance;
  }
  // This fails for component builds, however, production builds are not
  // component builds and we have not seen any production crashes here. If
  // crashes start appearing in production, we should re-evaluate the
  // FastPairRepository setup.
  CHECK(g_instance);
  return g_instance;
}

// static
std::string FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
    const std::string& account_key,
    const std::string& mac_address) {
  std::vector<uint8_t> concat_bytes(account_key.begin(), account_key.end());
  std::vector<uint8_t> mac_address_bytes(kBluetoothAddressSize);
  device::ParseBluetoothAddress(mac_address, mac_address_bytes);

  concat_bytes.insert(concat_bytes.end(), mac_address_bytes.begin(),
                      mac_address_bytes.end());
  std::array<uint8_t, crypto::kSHA256Length> hashed =
      crypto::SHA256Hash(concat_bytes);

  return std::string(hashed.begin(), hashed.end());
}

// static
void FastPairRepository::SetInstance(FastPairRepository* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

// static
void FastPairRepository::SetInstanceForTesting(FastPairRepository* instance) {
  g_test_instance = instance;
}

FastPairRepository::FastPairRepository() = default;
FastPairRepository::~FastPairRepository() = default;

}  // namespace quick_pair
}  // namespace ash
