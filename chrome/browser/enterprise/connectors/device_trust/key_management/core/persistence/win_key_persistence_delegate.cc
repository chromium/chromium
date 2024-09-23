// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/win_key_persistence_delegate.h"

#include <array>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/syslog_logging.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/metrics_utils.h"
#include "chrome/installer/util/install_util.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

namespace {

bool RecordFailure(KeyPersistenceOperation operation,
                   KeyPersistenceError error,
                   const std::string& log_message) {
  RecordError(operation, error);
  SYSLOG(ERROR) << log_message;
  return false;
}

// Creates the unexportable signing key given the key `trust_level`.
std::unique_ptr<crypto::UnexportableSigningKey> CreateSigningKey(
    KeyPersistenceDelegate::KeyTrustLevel trust_level) {
  std::unique_ptr<crypto::UnexportableKeyProvider> provider;

  if (trust_level == BPKUR::CHROME_BROWSER_HW_KEY) {
    provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  } else if (trust_level == BPKUR::CHROME_BROWSER_OS_KEY) {
    provider = std::make_unique<ECSigningKeyProvider>();
  }

  static constexpr std::array<crypto::SignatureVerifier::SignatureAlgorithm, 2>
      kAcceptableAlgorithms = {crypto::SignatureVerifier::ECDSA_SHA256,
                               crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  return provider ? provider->GenerateSigningKeySlowly(kAcceptableAlgorithms)
                  : nullptr;
}

}  // namespace

WinKeyPersistenceDelegate::~WinKeyPersistenceDelegate() = default;

bool WinKeyPersistenceDelegate::CheckRotationPermissions() {
  return true;
}

bool WinKeyPersistenceDelegate::StoreKeyPair(
    KeyPersistenceDelegate::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(false));
  if (!key.Valid()) {
    return RecordFailure(KeyPersistenceOperation::kStoreKeyPair,
                         KeyPersistenceError::kOpenPersistenceStorageFailed,
                         "Device trust key rotation failed. Could not open the "
                         "signing key storage for reading.");
  }

  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);
    if (key.DeleteValue(signingkey_name.c_str()) == ERROR_SUCCESS &&
        key.DeleteValue(trustlevel_name.c_str()) == ERROR_SUCCESS) {
      return true;
    }
    return RecordFailure(KeyPersistenceOperation::kStoreKeyPair,
                         KeyPersistenceError::kDeleteKeyPairFailed,
                         "Device trust key rotation failed. Failed to delete "
                         "the signing key pair.");
  }

  if (key.WriteValue(signingkey_name.c_str(), wrapped.data(), wrapped.size(),
                     REG_BINARY) == ERROR_SUCCESS &&
      key.WriteValue(trustlevel_name.c_str(), trust_level) == ERROR_SUCCESS) {
    return true;
  }

  return RecordFailure(KeyPersistenceOperation::kStoreKeyPair,
                       KeyPersistenceError::kWritePersistenceStorageFailed,
                       "Device trust key rotation failed. Could not write to "
                       "the signing key storage.");
}

scoped_refptr<SigningKeyPair> WinKeyPersistenceDelegate::LoadKeyPair(
    KeyStorageType type,
    LoadPersistedKeyResult* result) {
  base::win::RegKey key;
  std::wstring signingkey_name;
  std::wstring trustlevel_name;
  std::tie(key, signingkey_name, trustlevel_name) =
      InstallUtil::GetDeviceTrustSigningKeyLocation(
          InstallUtil::ReadOnly(true));
  if (!key.Valid()) {
    RecordFailure(KeyPersistenceOperation::kLoadKeyPair,
                  KeyPersistenceError::kOpenPersistenceStorageFailed,
                  "Device trust key rotation failed. Failed to open the "
                  "signing key storage for reading.");
    // TODO(b/301587025): Pipe error returned from opening the registry key for
    // better logging.
    return ReturnLoadKeyError(LoadPersistedKeyResult::kNotFound, result);
  }

  DWORD trust_level_dw;
  auto res = key.ReadValueDW(trustlevel_name.c_str(), &trust_level_dw);
  if (res != ERROR_SUCCESS) {
    RecordFailure(KeyPersistenceOperation::kLoadKeyPair,
                  KeyPersistenceError::kKeyPairMissingTrustLevel,
                  "Device trust key rotation failed. Failed to get the trust "
                  "level details from the signing key storage.");
    return ReturnLoadKeyError(LoadPersistedKeyResult::kNotFound, result);
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider;
  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (trust_level_dw == BPKUR::CHROME_BROWSER_HW_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_HW_KEY;
    provider = crypto::GetUnexportableKeyProvider(/*config=*/{});
  } else if (trust_level_dw == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
    provider = std::make_unique<ECSigningKeyProvider>();
  } else {
    RecordFailure(KeyPersistenceOperation::kLoadKeyPair,
                  KeyPersistenceError::kInvalidTrustLevel,
                  "Device trust key rotation failed. Invalid trust level for "
                  "the signing key.");
    return ReturnLoadKeyError(LoadPersistedKeyResult::kMalformedKey, result);
  }

  std::vector<uint8_t> wrapped;
  DWORD reg_type = REG_NONE;
  DWORD size = 0;
  res = key.ReadValue(signingkey_name.c_str(), nullptr, &size, &reg_type);
  if (res == ERROR_SUCCESS && reg_type == REG_BINARY) {
    wrapped.resize(size);
    res = key.ReadValue(signingkey_name.c_str(), wrapped.data(), &size,
                        &reg_type);
  }
  if (res != ERROR_SUCCESS) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kKeyPairMissingSigningKey,
        "Device trust key rotation failed. Failed to get the signing key "
        "details from the signing key storage.");
    return ReturnLoadKeyError(LoadPersistedKeyResult::kNotFound, result);
  }

  if (reg_type != REG_BINARY) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kInvalidSigningKey,
        "Device trust key rotation failed. The signing key type is incorrect.");
    return ReturnLoadKeyError(LoadPersistedKeyResult::kMalformedKey, result);
  }

  auto signing_key = provider->FromWrappedSigningKeySlowly(wrapped);
  if (!signing_key) {
    RecordFailure(
        KeyPersistenceOperation::kLoadKeyPair,
        KeyPersistenceError::kCreateSigningKeyFromWrappedFailed,
        "Device trust key rotation failed. Failure creating a signing key "
        "object from the signing key details.");
    return ReturnLoadKeyError(LoadPersistedKeyResult::kMalformedKey, result);
  }

  if (result) {
    *result = LoadPersistedKeyResult::kSuccess;
  }
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              trust_level);
}

scoped_refptr<SigningKeyPair> WinKeyPersistenceDelegate::CreateKeyPair() {
  // Attempt to create a TPM signing key.
  KeyPersistenceDelegate::KeyTrustLevel trust_level =
      BPKUR::CHROME_BROWSER_HW_KEY;
  auto signing_key = CreateSigningKey(trust_level);

  // Fallback to an OS signing key when a TPM key cannot be created.
  if (!signing_key) {
    RecordFailure(KeyPersistenceOperation::kCreateKeyPair,
                  KeyPersistenceError::kGenerateHardwareSigningKeyFailed,
                  "Device trust key rotation failed. Failed to generate a new "
                  "hardware signing key.");

    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
    signing_key = CreateSigningKey(trust_level);
  }

  if (!signing_key) {
    RecordFailure(KeyPersistenceOperation::kCreateKeyPair,
                  KeyPersistenceError::kGenerateOSSigningKeyFailed,
                  "Device trust key rotation failed. Failed to generate a new "
                  "OS signing key.");
    return nullptr;
  }

  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              trust_level);
}

bool WinKeyPersistenceDelegate::PromoteTemporaryKeyPair() {
  // TODO(b/290068551): Implement this method.
  return true;
}

bool WinKeyPersistenceDelegate::DeleteKeyPair(KeyStorageType type) {
  // TODO(b/290068551): Implement this method.
  return true;
}

}  // namespace enterprise_connectors
