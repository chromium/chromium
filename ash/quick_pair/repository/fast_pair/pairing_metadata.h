// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PAIRING_METADATA_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PAIRING_METADATA_H_

#include <vector>

#include "base/memory/raw_ptr.h"

namespace ash {
namespace quick_pair {

class DeviceMetadata;

// Thin wrapper around Account Key + decoded metadata for a Fast Pair device
// which has already been paired.
struct PairingMetadata {
  explicit PairingMetadata(DeviceMetadata* device_metadata,
                           std::vector<uint8_t> account_key);
  PairingMetadata(const PairingMetadata&);
  PairingMetadata& operator=(const PairingMetadata&);
  PairingMetadata(PairingMetadata&&);
  ~PairingMetadata();

  raw_ptr<DeviceMetadata, DanglingUntriaged> device_metadata;
  std::vector<uint8_t> account_key;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PAIRING_METADATA_H_
