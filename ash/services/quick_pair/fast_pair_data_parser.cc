// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/fast_pair_data_parser.h"

#include <algorithm>

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/services/quick_pair/fast_pair_decryption.h"
#include "ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "crypto/openssl_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

bool ValidateInputSizes(const std::vector<uint8_t>& aes_key_bytes,
                        const std::vector<uint8_t>& encrypted_bytes) {
  if (aes_key_bytes.size() != kAesBlockByteSize) {
    QP_LOG(WARNING) << __func__
                    << ": AES key should have size = " << kAesBlockByteSize
                    << ", actual =  " << aes_key_bytes.size();
    return false;
  }

  if (encrypted_bytes.size() != kEncryptedDataByteSize) {
    QP_LOG(WARNING) << __func__ << ": Encrypted bytes should have size = "
                    << kEncryptedDataByteSize
                    << ", actual =  " << encrypted_bytes.size();
    return false;
  }

  return true;
}

void ConvertVectorsToArrays(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_bytes,
    std::array<uint8_t, kAesBlockByteSize>& out_aes_key_bytes,
    std::array<uint8_t, kEncryptedDataByteSize>& out_encrypted_bytes) {
  std::copy(aes_key_bytes.begin(), aes_key_bytes.end(),
            out_aes_key_bytes.begin());
  std::copy(encrypted_bytes.begin(), encrypted_bytes.end(),
            out_encrypted_bytes.begin());
}

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDataParser::FastPairDataParser(
    mojo::PendingReceiver<mojom::FastPairDataParser> receiver)
    : receiver_(this, std::move(receiver)) {
  crypto::EnsureOpenSSLInit();
}

FastPairDataParser::~FastPairDataParser() = default;

void FastPairDataParser::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback) {
  std::move(callback).Run(
      fast_pair_decoder::HasModelId(&service_data)
          ? fast_pair_decoder::GetHexModelIdFromServiceData(&service_data)
          : absl::nullopt);
}

void FastPairDataParser::ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptedResponseCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_response_bytes)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_response_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedResponse(key, bytes));
}

void FastPairDataParser::ParseDecryptedPasskey(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    ParseDecryptedPasskeyCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_passkey_bytes)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_passkey_bytes, key, bytes);

  std::move(callback).Run(
      fast_pair_decryption::ParseDecryptedPasskey(key, bytes));
}

}  // namespace quick_pair
}  // namespace ash
