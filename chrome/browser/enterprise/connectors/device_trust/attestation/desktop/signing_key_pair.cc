// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include <vector>

#include "build/build_config.h"

#if defined(OS_WIN)

#include "base/base64.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/install_static/install_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "crypto/unexportable_key.h"

#else

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"

#endif  // !defined(OS_WIN)

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

#if defined(OS_WIN)

constexpr wchar_t kSigningKeyName[] = L"signing_key";
constexpr wchar_t kTrustLevelName[] = L"trust_level";

// Gets the registry path to store the device trust values.
std::wstring GetRegistryPath() {
  return install_static::GetRegistryPath() + L"\\DeviceTrust";
}

#endif

}  // namespace

SigningKeyPair::SigningKeyPair() {
  Init();
}
SigningKeyPair::~SigningKeyPair() = default;

bool SigningKeyPair::SignMessage(const std::string& message,
                                 std::string* signature) {
  if (!key_pair_)
    return false;

  auto signature_bytes =
      key_pair_->SignSlowly(base::as_bytes(base::make_span(message)));
  if (signature_bytes)
    *signature = std::string(signature_bytes->begin(), signature_bytes->end());

  return signature_bytes.has_value();
}

bool SigningKeyPair::ExportPublicKey(std::vector<uint8_t>* public_key) {
  if (!key_pair_)
    return false;

  *public_key = key_pair_->GetSubjectPublicKeyInfo();
  return public_key->size() != 0;
}

bool SigningKeyPair::Rotate() {
  // TODO(b/194384757): Implement for windows.
  // TODO(b/194385359): Implement for mac.
  // TODO(b/194385515): Implement for linux.
  return false;
}

bool SigningKeyPair::RotateWithElevation(const std::string& dm_token_base64) {
  std::string dm_token;
  if (!base::Base64Decode(dm_token_base64, &dm_token))
    return false;

  // Create a new key pair.  First try creating a TPM-backed key.  If that does
  // not work, try a less secure type.
  KeyTrustLevel new_trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  auto acceptable_algorithms = {
      crypto::SignatureVerifier::ECDSA_SHA256,
      crypto::SignatureVerifier::RSA_PKCS1_SHA256,
  };

  // TODO(b/194385671): Impl TPM backed keys on Linux.
#if defined(OS_WIN)
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      crypto::GetUnexportableKeyProvider();
#else
  std::unique_ptr<crypto::UnexportableKeyProvider> provider;
#endif

  auto new_key_pair =
      provider ? provider->GenerateSigningKeySlowly(acceptable_algorithms)
               : nullptr;
  if (new_key_pair) {
    new_trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  } else {
    new_trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
    ECSigningKeyProvider provider;
    new_key_pair = provider.GenerateSigningKeySlowly(acceptable_algorithms);
  }
  if (!new_key_pair)
    return false;

  // Get the pubkey of the new key pair.
  std::vector<uint8_t> pubkey = new_key_pair->GetSubjectPublicKeyInfo();

  // If there is an existing key, sign the new pubkey.  Otherwise send an
  // empty signature to DM server.
  absl::optional<std::vector<uint8_t>> signature;
  if (key_pair_) {
    signature = key_pair_->SignSlowly(pubkey);
    if (!signature.has_value())
      return false;
  }

  // TODO(b/195447899): send pubkey and signature to DM server.

  key_pair_ = std::move(new_key_pair);
  trust_level_ = new_trust_level;

  return StoreKeyPair();
}

void SigningKeyPair::SetKeyPairForTesting(
    std::unique_ptr<crypto::UnexportableSigningKey> key_pair) {
  key_pair_ = std::move(key_pair);
}

void SigningKeyPair::Init() {
#if defined(OS_WIN)
  LoadKeyPair();
#else
  // No key pair stored.
  if (!LoadKeyPair()) {
    auto acceptable_algorithms = {
        crypto::SignatureVerifier::ECDSA_SHA256,
        crypto::SignatureVerifier::RSA_PKCS1_SHA256,
    };
    ECSigningKeyProvider key_provider;
    key_pair_ = key_provider.GenerateSigningKeySlowly(acceptable_algorithms);
    DCHECK(key_pair_);
    StoreKeyPair();
  }
#endif  // OS_WIN
}

bool SigningKeyPair::StoreKeyPair() {
#if defined(OS_WIN)
  DCHECK_NE(nullptr, key_pair_.get());
  std::vector<uint8_t> wrapped = key_pair_->GetWrappedKey();

  // Write the wrapped key's bytes to the HKLM register.  Note that this will
  // only work if the calling process is running elevated.
  base::win::RegKey key(HKEY_LOCAL_MACHINE, GetRegistryPath().c_str(),
                        KEY_SET_VALUE);
  if (!key.Valid())
    return false;

  auto res = key.WriteValue(kSigningKeyName, wrapped.data(), wrapped.size(),
                            REG_BINARY);
  if (res != ERROR_SUCCESS)
    return false;

  res = key.WriteValue(kTrustLevelName, trust_level_);
  return res == ERROR_SUCCESS;
#else
  if (!key_pair_) {
    DVLOG(1) << "No `key_pair_` member";
    return false;
  }

  std::vector<uint8_t> wrapped = key_pair_->GetWrappedKey();
  if (wrapped.size() == 0) {
    DVLOG(1) << "Could not export the private key.";
    return false;
  }

  std::string encrypted_key;
  bool encrypted = OSCrypt::EncryptString(
      std::string(wrapped.begin(), wrapped.end()), &encrypted_key);
  if (!encrypted) {
    DVLOG(1) << "Error while encrypting the private key.";
    return false;
  }
  std::string encoded_encrypted_key;
  base::Base64Encode(encrypted_key, &encoded_encrypted_key);

  PrefService* const local_state = g_browser_process->local_state();
  local_state->SetString(enterprise_connectors::kDeviceTrustPrivateKeyPref,
                         encoded_encrypted_key);
  return true;
#endif  // OS_WIN
}

bool SigningKeyPair::LoadKeyPair() {
#if defined(OS_WIN)
  base::win::RegKey key(HKEY_LOCAL_MACHINE, GetRegistryPath().c_str(),
                        KEY_QUERY_VALUE);
  if (!key.Valid())
    return false;

  std::vector<uint8_t> wrapped;
  DWORD type = REG_NONE;
  DWORD size = 0;
  auto res = key.ReadValue(kSigningKeyName, nullptr, &size, &type);
  if (res == ERROR_SUCCESS && type == REG_BINARY) {
    wrapped.resize(size);
    res = key.ReadValue(kSigningKeyName, wrapped.data(), &size, &type);
  }
  if (res != ERROR_SUCCESS || type != REG_BINARY)
    return false;

  DWORD trust_level;
  res = key.ReadValueDW(kTrustLevelName, &trust_level);
  if (res != ERROR_SUCCESS)
    return false;

  if (trust_level == BPKUR::CHROME_BROWSER_TPM_KEY) {
    trust_level_ = BPKUR::CHROME_BROWSER_TPM_KEY;
    key_pair_ =
        crypto::GetUnexportableKeyProvider()->FromWrappedSigningKeySlowly(
            wrapped);
  } else if (trust_level == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level_ = BPKUR::CHROME_BROWSER_OS_KEY;

    ECSigningKeyProvider provider;
    key_pair_ = provider.FromWrappedSigningKeySlowly(wrapped);
  } else {
    return false;
  }

  return key_pair_.get() != nullptr;
#else
  PrefService* const local_state = g_browser_process->local_state();
  std::string base64_encrypted_private_key_info =
      local_state->GetString(enterprise_connectors::kDeviceTrustPrivateKeyPref);
  // No key pair stored.
  if (base64_encrypted_private_key_info.empty())
    return false;

  std::string decoded_encrypted_key;
  if (!base::Base64Decode(base64_encrypted_private_key_info,
                          &decoded_encrypted_key)) {
    DVLOG(1) << "Error while decoding base64 encrypted key.";
    return false;
  }

  std::string decrypted_private_key_info;
  if (!OSCrypt::DecryptString(decoded_encrypted_key,
                              &decrypted_private_key_info)) {
    DVLOG(1) << "Error while decrypting the private key";
    return false;
  }

  std::vector<uint8_t> wrapped(decrypted_private_key_info.begin(),
                               decrypted_private_key_info.end());
  ECSigningKeyProvider key_provider;
  key_pair_ = key_provider.FromWrappedSigningKeySlowly(wrapped);
  if (!key_pair_) {
    DVLOG(1) << "Could not create from private key info";
    return false;
  }
  return true;
#endif  // OS_WIN
}

}  // namespace enterprise_connectors
