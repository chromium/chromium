// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/eche_app_ui/eche_uid_provider.h"

#include <openssl/base64.h>

#include <cstring>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace ash {
namespace eche_app {

const char kEcheAppSeedPref[] = "cros.echeapp.seed";
const size_t kSeedSizeInByte = 32;

EcheUidProvider::EcheUidProvider(PrefService* pref_service)
    : pref_service_(pref_service) {}

EcheUidProvider::~EcheUidProvider() = default;

void EcheUidProvider::GetUid(
    base::OnceCallback<void(const std::string&)> callback) {
  PA_LOG(INFO) << "echeapi EcheUidProvider GetUid";
  if (!uid_.empty()) {
    std::move(callback).Run(uid_);
    return;
  }
  uint8_t public_key[ED25519_PUBLIC_KEY_LEN];
  uint8_t private_key[ED25519_PRIVATE_KEY_LEN];
  std::string pref_seed = pref_service_->GetString(kEcheAppSeedPref);
  if (pref_seed.empty()) {
    GenerateKeyPair(public_key, private_key);
  } else {
    std::optional<std::vector<uint8_t>> result =
        ConvertStringToBinary(pref_seed, kSeedSizeInByte);
    if (!result) {
      PA_LOG(WARNING) << "Invalid encoded string, regenerate the keypair.";
      GenerateKeyPair(public_key, private_key);
    } else {
      DCHECK_EQ(kSeedSizeInByte, result->size());
      ED25519_keypair_from_seed(public_key, private_key, result->data());
    }
  }
  uid_ = ConvertBinaryToString(public_key);
  std::move(callback).Run(uid_);
}

void EcheUidProvider::GenerateKeyPair(
    uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
    uint8_t private_key[ED25519_PRIVATE_KEY_LEN]) {
  ED25519_keypair(public_key, private_key);
  // Store the seed (what RFC8032 calls a private key), which is the
  // first 32 bytes of what BoringSSL calls the private key.
  pref_service_->SetString(
      kEcheAppSeedPref,
      ConvertBinaryToString(base::make_span(private_key, kSeedSizeInByte)));
}

std::optional<std::vector<uint8_t>> EcheUidProvider::ConvertStringToBinary(
    std::string_view str,
    size_t expected_len) {
  std::vector<uint8_t> decoded_data(str.size());
  size_t decoded_data_len = 0;
  if (!EVP_DecodeBase64(
          decoded_data.data(), &decoded_data_len, decoded_data.size(),
          reinterpret_cast<const uint8_t*>(str.data()), str.size())) {
    PA_LOG(ERROR) << "Attempting to decode string failed.";
    return std::nullopt;
  }
  if (decoded_data_len != expected_len) {
    PA_LOG(ERROR) << "Expected length is not match.";
    return std::nullopt;
  }
  decoded_data.resize(decoded_data_len);
  return decoded_data;
}

std::string EcheUidProvider::ConvertBinaryToString(
    base::span<const uint8_t> src) {
  // Use a constant-time implementation of base64 in BoringSSL instead of
  // base::Base64Encode.
  size_t encoded_data_len;
  CHECK(EVP_EncodedLength(&encoded_data_len, src.size()) == 1);
  std::vector<uint8_t> encoded_data(encoded_data_len);
  size_t encoded_block_len =
      EVP_EncodeBlock(encoded_data.data(), src.data(), src.size());
  // The return value of EVP_EncodeBlock is not the same with the result of
  // EVP_EncodedLength, we save the real size of EVP_EncodeBlock to string, not
  // the size of buffer.
  return std::string(reinterpret_cast<const char*>(encoded_data.data()),
                     encoded_block_len);
}

void EcheUidProvider::Bind(
    mojo::PendingReceiver<mojom::UidGenerator> receiver) {
  uid_receiver_.reset();
  uid_receiver_.Bind(std::move(receiver));
}

}  // namespace eche_app
}  // namespace ash
