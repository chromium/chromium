// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_

#include <stdint.h>

#include <array>
#include <string>

#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

constexpr int kBlockByteSize = 16;

COMPONENT_EXPORT(QUICK_PAIR_PAIRING)
absl::optional<KeyPair> GenerateKeysWithEcdhKeyAgreement(
    const std::string& decoded_public_anti_spoofing);

COMPONENT_EXPORT(QUICK_PAIR_PAIRING)
const std::array<uint8_t, kBlockByteSize> EncryptBytes(
    const std::array<uint8_t, kBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kBlockByteSize>& bytes_to_encrypt);

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_
