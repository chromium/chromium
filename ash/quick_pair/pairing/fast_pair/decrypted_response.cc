// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/decrypted_response.h"

namespace ash {
namespace quick_pair {

DecryptedResponse::DecryptedResponse(
    uint8_t message_type,
    std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes,
    std::array<uint8_t, kDecryptedResponseSaltByteSize> salt)
    : message_type(message_type), address_bytes(address_bytes), salt(salt) {}

DecryptedResponse::DecryptedResponse(DecryptedResponse&&) = default;

}  // namespace quick_pair
}  // namespace ash
