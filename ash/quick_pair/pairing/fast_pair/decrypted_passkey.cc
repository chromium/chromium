// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/decrypted_passkey.h"

namespace ash {
namespace quick_pair {

DecryptedPasskey::DecryptedPasskey(
    uint8_t message_type,
    uint32_t passkey,
    std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt)
    : message_type(message_type), passkey(passkey), salt(salt) {}

DecryptedPasskey::DecryptedPasskey(DecryptedPasskey&&) = default;

}  // namespace quick_pair
}  // namespace ash
