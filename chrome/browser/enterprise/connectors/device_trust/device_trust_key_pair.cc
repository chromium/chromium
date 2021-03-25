// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "crypto/ec_signature_creator.h"

namespace enterprise_connectors {

namespace {

const char kBeginPrivateKey[] = "-----BEGIN PRIVATE KEY-----\n";
const char kEndPrivateKey[] = "-----END PRIVATE KEY-----\n";
const char kBeginPublicKey[] = "-----BEGIN PUBLIC KEY-----\n";
const char kEndPublicKey[] = "-----END PUBLIC KEY-----\n";

enum class KeyType { PRIVATE_KEY, PUBLIC_KEY };

std::string BytesToEncodedString(const std::vector<uint8_t>& bytes) {
  std::string encoded_string = "";
  base::Base64UrlEncode(std::string(bytes.begin(), bytes.end()),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);
  return encoded_string;
}

std::vector<uint8_t> EncodedStringToBytes(
    const base::StringPiece& encoded_string) {
  std::string decoded_string;
  if (!base::Base64UrlDecode(encoded_string,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_string)) {
    return std::vector<uint8_t>();
  }
  return std::vector<uint8_t>(decoded_string.begin(), decoded_string.end());
}

bool CreatePEMKey(const base::StringPiece& raw_key,
                  KeyType key_type,
                  std::string& pem) {
  std::string encoded_key;
  base::Base64UrlEncode(raw_key, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_key);
  pem = "";
  switch (key_type) {
    case KeyType::PRIVATE_KEY:
      pem += kBeginPrivateKey;
      pem += encoded_key + "\n";
      pem += kEndPrivateKey;
      break;
    case KeyType::PUBLIC_KEY:
      pem += kBeginPublicKey;
      pem += encoded_key + "\n";
      pem += kEndPublicKey;
      break;
  }
  return true;
}

}  // namespace

DeviceTrustKeyPair::DeviceTrustKeyPair() = default;

DeviceTrustKeyPair::~DeviceTrustKeyPair() = default;

bool DeviceTrustKeyPair::Init() {
  PrefService* const local_state = g_browser_process->local_state();

  std::string base64_encrypted_private_key_info =
      local_state->GetString(enterprise_connectors::kDeviceTrustPrivateKeyPref);

  // No key pair stored.
  if (base64_encrypted_private_key_info.empty()) {
    key_pair_ = crypto::ECPrivateKey::Create();
    return StoreKeyPair();
  }

  std::string decoded;
  if (!base::Base64Decode(base64_encrypted_private_key_info, &decoded)) {
    // TODO(crbug/1176175): Handle error for when it can't get the private
    // key.
    LOG(ERROR) << "[DeviceTrust - Init] error while decoding the private key";
    return false;
  }

  std::string decrypted_private_key_info;
  bool success = OSCrypt::DecryptString(decoded, &decrypted_private_key_info);
  if (!success) {
    // TODO(crbug/1176175): Handle error for when it can't get the private
    // key.
    LOG(ERROR) << "[DeviceTrust - Init] error while decrypting the private key";
    return false;
  }

  key_pair_ = LoadFromPrivateKeyInfo(decrypted_private_key_info);
  if (!key_pair_)
    return false;
  return true;
}

bool DeviceTrustKeyPair::StoreKeyPair() {
  if (!key_pair_) {
    LOG(ERROR) << "[DeviceTrust - StoreKeyPair] no `key_pair_` member";
    return false;
  }

  PrefService* const local_state = g_browser_process->local_state();
  // Add private encoded encrypted private key to local_state.
  std::vector<uint8_t> private_key;
  if (!key_pair_->ExportPrivateKey(&private_key))
    return false;

  std::string encoded_private_key = BytesToEncodedString(private_key);
  std::string encrypted_key;
  bool encrypted = OSCrypt::EncryptString(encoded_private_key, &encrypted_key);
  if (!encrypted) {
    LOG(ERROR) << "[DeviceTrust - StoreKeyPair] error while encrypting the "
                  "private key.";
    return false;
  }
  // The string must be encoded as base64 for storage in local state.
  std::string encoded;
  base::Base64Encode(encrypted_key, &encoded);
  local_state->SetString(enterprise_connectors::kDeviceTrustPrivateKeyPref,
                         encoded);

  // Add public key to local_state.
  local_state->SetString(enterprise_connectors::kDeviceTrustPublicKeyPref,
                         ExportPEMPublicKey());

  return true;
}

std::unique_ptr<crypto::ECPrivateKey>
DeviceTrustKeyPair::LoadFromPrivateKeyInfo(
    const std::string& private_key_info_block) {
  std::vector<uint8_t> private_key_info =
      EncodedStringToBytes(private_key_info_block);
  return crypto::ECPrivateKey::CreateFromPrivateKeyInfo(private_key_info);
}

std::string DeviceTrustKeyPair::ExportPEMPrivateKey() {
  std::vector<uint8_t> private_key_pkcs8;
  key_pair_->ExportPrivateKey(&private_key_pkcs8);
  std::string private_key;
  if (!CreatePEMKey(BytesToEncodedString(private_key_pkcs8),
                    KeyType::PRIVATE_KEY, private_key))
    return std::string();
  return private_key;
}

bool DeviceTrustKeyPair::ExportPublicKey(std::vector<uint8_t>* public_key) {
  if (!key_pair_->ExportPublicKey(public_key))
    return false;
  return true;
}

std::string DeviceTrustKeyPair::ExportPEMPublicKey() {
  std::string raw_public_key;
  if (!key_pair_->ExportRawPublicKey(&raw_public_key))
    return std::string();
  std::string public_key;
  if (!CreatePEMKey(raw_public_key, KeyType::PUBLIC_KEY, public_key))
    return std::string();
  return public_key;
}

}  // namespace enterprise_connectors
