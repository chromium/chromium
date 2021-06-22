// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

#include "base/base64.h"
#include "base/containers/span.h"
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

DeviceTrustKeyPair::DeviceTrustKeyPair() = default;

DeviceTrustKeyPair::~DeviceTrustKeyPair() = default;

bool DeviceTrustKeyPair::Init() {
  // No key pair stored.
  if (!LoadKeyPair()) {
    key_pair_ = crypto::ECPrivateKey::Create();
    return StoreKeyPair();
  }
  return true;
}

bool DeviceTrustKeyPair::StoreKeyPair() {
  if (!key_pair_) {
    LOG(ERROR) << "No `key_pair_` member";
    return false;
  }

  PrefService* const local_state = g_browser_process->local_state();
  // Add encoded encrypted private key to local_state.
  std::vector<uint8_t> private_key;
  if (!key_pair_->ExportPrivateKey(&private_key)) {
    LOG(ERROR) << "Could not export the private key.";
    return false;
  }
  std::string encrypted_key;
  bool encrypted = OSCrypt::EncryptString(
      std::string(private_key.begin(), private_key.end()), &encrypted_key);
  if (!encrypted) {
    LOG(ERROR) << "Error while encrypting the private key.";
    return false;
  }
  std::string encoded_encrypted_key;
  base::Base64Encode(encrypted_key, &encoded_encrypted_key);
  local_state->SetString(enterprise_connectors::kDeviceTrustPrivateKeyPref,
                         encoded_encrypted_key);
  return true;
}

bool DeviceTrustKeyPair::LoadKeyPair() {
  PrefService* const local_state = g_browser_process->local_state();
  std::string base64_encrypted_private_key_info =
      local_state->GetString(enterprise_connectors::kDeviceTrustPrivateKeyPref);
  // No key pair stored.
  if (base64_encrypted_private_key_info.empty())
    return false;

  std::string decoded_encrypted_key;
  if (!base::Base64Decode(base64_encrypted_private_key_info,
                          &decoded_encrypted_key))
    LOG(ERROR) << "Error while decoding base64 encrypted key.";

  std::string decrypted_private_key_info;
  if (!OSCrypt::DecryptString(decoded_encrypted_key,
                              &decrypted_private_key_info)) {
    LOG(ERROR) << "Error while decrypting the private key";
    return false;
  }

  // Load private key from a PrivateKeyInfo value.
  std::vector<uint8_t> private_key_info(decrypted_private_key_info.begin(),
                                        decrypted_private_key_info.end());
  key_pair_ = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(private_key_info);
  if (!key_pair_) {
    LOG(ERROR) << "Could not create from private key info";
    return false;
  }
  return true;
}

bool DeviceTrustKeyPair::ExportPublicKey(std::vector<uint8_t>* public_key) {
  return key_pair_->ExportPublicKey(public_key);
}

bool DeviceTrustKeyPair::SignMessage(const std::string& message,
                                     std::string* signature) {
  std::vector<uint8_t> signature_bytes;
  // Create the signer which uses SHA256.
  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key_pair_.get()));
  // Make the signature.
  if (!signer->Sign(reinterpret_cast<const uint8_t*>(message.c_str()),
                    message.size(), &signature_bytes))
    return false;
  *signature = std::string(signature_bytes.begin(), signature_bytes.end());
  return true;
}

}  // namespace enterprise_connectors
