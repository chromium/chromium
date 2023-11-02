// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"

namespace ash {
namespace quick_pair {

PairingMetadata::PairingMetadata(DeviceMetadata* device_metadata,
                                 std::vector<uint8_t> account_key)
    : device_metadata(device_metadata), account_key(std::move(account_key)) {}

PairingMetadata::PairingMetadata(const PairingMetadata&) = default;
PairingMetadata& PairingMetadata::operator=(const PairingMetadata&) = default;
PairingMetadata::PairingMetadata(PairingMetadata&&) = default;
PairingMetadata::~PairingMetadata() = default;

}  // namespace quick_pair
}  // namespace ash
