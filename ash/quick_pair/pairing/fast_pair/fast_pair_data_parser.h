// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_PARSER_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr int kEncryptedDataByteSize = 16;
constexpr int kAesBlockByteSize = 16;

}  // namespace

namespace ash {
namespace quick_pair {

// This parses the encrypted bytes from the Bluetooth device.
class FastPairDataParser {
 public:
  FastPairDataParser();
  ~FastPairDataParser();
  FastPairDataParser(const FastPairDataParser&) = delete;
  FastPairDataParser& operator=(const FastPairDataParser&) = delete;

  absl::optional<DecryptedResponse> ParseDecryptedResponse(
      const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
      const std::array<uint8_t, kEncryptedDataByteSize>&
          encrypted_response_bytes);

  absl::optional<DecryptedPasskey> ParseDecryptedPasskey(
      const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
      const std::array<uint8_t, kEncryptedDataByteSize>&
          encrypted_passkey_bytes);

 private:
  std::array<uint8_t, kEncryptedDataByteSize> DecryptBytes(
      const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
      const std::array<uint8_t, kEncryptedDataByteSize>& encrypted_bytes);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_PARSER_H_
