// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_RESPONSE_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_RESPONSE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/component_export.h"

namespace {

constexpr int kDecryptedResponseAddressByteSize = 6;
constexpr int kDecryptedResponseSaltByteSize = 9;

}  // namespace

namespace ash {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a decrypted response.
struct DecryptedResponse {
  DecryptedResponse(
      uint8_t message_type,
      std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes,
      std::array<uint8_t, kDecryptedResponseSaltByteSize> salt);
  DecryptedResponse(const DecryptedResponse&) = delete;
  DecryptedResponse(DecryptedResponse&&);
  DecryptedResponse& operator=(const DecryptedResponse&) = delete;
  DecryptedResponse& operator=(DecryptedResponse&&) = delete;
  ~DecryptedResponse() = default;

  const uint8_t message_type;
  const std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  const std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_RESPONSE_H_
