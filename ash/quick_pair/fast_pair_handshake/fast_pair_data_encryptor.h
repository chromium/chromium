// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

inline constexpr int kBlockSizeBytes = 16;

namespace ash {
namespace quick_pair {

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptor {
 public:
  // Encrypts bytes with the stored secret key.
  virtual const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) = 0;

  virtual const absl::optional<std::array<uint8_t, 64>>& GetPublicKey() = 0;

  // Decrypt and parse decrypted response bytes with the stored secret key.
  virtual void ParseDecryptedResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      base::OnceCallback<void(const absl::optional<DecryptedResponse>&)>
          callback) = 0;

  // Decrypt and parse decrypted passkey bytes with the stored secret key.
  virtual void ParseDecryptedPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      base::OnceCallback<void(const absl::optional<DecryptedPasskey>&)>
          callback) = 0;

  virtual ~FastPairDataEncryptor() = default;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
