// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/payload_util.h"

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "crypto/aead.h"
#include "crypto/random.h"

namespace tabs {

namespace {
// Constants for AES_128_CTR_HMAC_SHA256.
constexpr size_t kKeyLength = 48;
constexpr size_t kNonceLength = 12;
}  // namespace

std::vector<uint8_t> GenerateKeyForOtrPayloads() {
  DCHECK_EQ(kKeyLength,
            crypto::aead::KeySizeFor(crypto::aead::AES_128_CTR_HMAC_SHA256));
  return crypto::RandBytesAsVector(kKeyLength);
}

std::vector<uint8_t> SealPayload(base::span<const uint8_t> key,
                                 base::span<const uint8_t> payload,
                                 StorageId storage_id) {
  DCHECK_EQ(kNonceLength,
            crypto::aead::NonceSizeFor(crypto::aead::AES_128_CTR_HMAC_SHA256));
  auto nonce = crypto::RandBytesAsArray<kNonceLength>();
  auto bytes = crypto::aead::Seal(crypto::aead::AES_128_CTR_HMAC_SHA256, key,
                                  payload, nonce, storage_id.AsBytes());

  bytes.reserve(payload.size() + nonce.size());
  bytes.insert(bytes.end(), nonce.begin(), nonce.end());
  return bytes;
}

std::optional<std::vector<uint8_t>> OpenPayload(
    base::span<const uint8_t> key,
    base::span<const uint8_t> encrypted_payload,
    StorageId storage_id) {
  const size_t nonce_length =
      crypto::aead::NonceSizeFor(crypto::aead::AES_128_CTR_HMAC_SHA256);
  if (encrypted_payload.size() < nonce_length) {
    LOG(WARNING) << "Sealed payload is missing nonce.";
    return std::nullopt;
  }

  size_t payload_end = encrypted_payload.size() - nonce_length;
  auto [payload, nonce] = encrypted_payload.split_at(payload_end);
  std::optional<std::vector<uint8_t>> output =
      crypto::aead::Open(crypto::aead::AES_128_CTR_HMAC_SHA256, key, payload,
                         nonce, storage_id.AsBytes());
  LOG_IF(WARNING, !output) << "Failed to open sealed payload.";
  return output;
}

}  // namespace tabs
