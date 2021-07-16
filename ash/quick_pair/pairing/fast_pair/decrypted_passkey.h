// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_PASSKEY_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_PASSKEY_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/component_export.h"

namespace {

constexpr int kDecryptedPasskeySaltByteSize = 12;

}  // namespace

namespace ash {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a decrypted account passkey.
struct DecryptedPasskey {
  DecryptedPasskey(uint8_t message_type,
                   uint32_t passkey,
                   std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt);
  DecryptedPasskey(const DecryptedPasskey&) = delete;
  DecryptedPasskey(DecryptedPasskey&&);
  DecryptedPasskey& operator=(const DecryptedPasskey&) = delete;
  DecryptedPasskey& operator=(DecryptedPasskey&&) = delete;
  ~DecryptedPasskey() = default;

  const uint8_t message_type;
  const uint32_t passkey;
  const std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_DECRYPTED_PASSKEY_H_
