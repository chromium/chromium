// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"

#include <Security/Security.h>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

MacKeyPersistenceDelegate::MacKeyPersistenceDelegate()
    : client_(SecureEnclaveClient::Create()) {
  DCHECK(client_);
}

MacKeyPersistenceDelegate::~MacKeyPersistenceDelegate() = default;

bool MacKeyPersistenceDelegate::CheckRotationPermissions() {
  return true;
}

bool MacKeyPersistenceDelegate::StoreKeyPair(KeyTrustLevel trust_level,
                                             std::vector<uint8_t> wrapped) {
  // This method does not actually store a new key pair but instead performs the
  // rollback of moving the previous signing key from temporary key storage back
  // to permanent key storage when failure occurs during the key rotation. This
  // is because key storage is handled by the SecureEnclaveSigningKey upon key
  // creation.
  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);

    // A previous signing key did not exist so the newly created signing key
    // stored in the permanent key storage is deleted.
    return client_->DeleteKey(SecureEnclaveClient::KeyType::kPermanent);
  }

  auto key_type = SecureEnclaveClient::GetTypeFromWrappedKey(wrapped);

  if (!key_type ||
      key_type.value() == SecureEnclaveClient::KeyType::kTemporary) {
    // Roll the previous signing key back to permanent key storage.
    return client_->UpdateStoredKeyLabel(
        SecureEnclaveClient::KeyType::kTemporary,
        SecureEnclaveClient::KeyType::kPermanent);
  }

  return true;
}

scoped_refptr<SigningKeyPair> MacKeyPersistenceDelegate::LoadKeyPair(
    KeyStorageType type,
    LoadPersistedKeyResult* result) {
  SecureEnclaveSigningKeyProvider provider;
  OSStatus error;
  auto signing_key = provider.LoadStoredSigningKeySlowly(
      SecureEnclaveClient::KeyType::kPermanent, &error);
  if (!signing_key) {
    LoadPersistedKeyResult error_result =
        error == errSecItemNotFound ? LoadPersistedKeyResult::kNotFound
                                    : LoadPersistedKeyResult::kUnknown;
    return ReturnLoadKeyError(error_result, result);
  }

  if (result) {
    *result = LoadPersistedKeyResult::kSuccess;
  }
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              BPKUR::CHROME_BROWSER_HW_KEY);
}

scoped_refptr<SigningKeyPair> MacKeyPersistenceDelegate::CreateKeyPair() {
  // Moving a previous signing key to temporary key storage if a key exists.
  client_->UpdateStoredKeyLabel(SecureEnclaveClient::KeyType::kPermanent,
                                SecureEnclaveClient::KeyType::kTemporary);

  // The permanent key provider creates a new signing key pair in the permanent
  // key storage.
  SecureEnclaveSigningKeyProvider provider;
  auto signing_key = provider.GenerateSigningKeySlowly();
  if (!signing_key) {
    return nullptr;
  }

  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              BPKUR::CHROME_BROWSER_HW_KEY);
}

bool MacKeyPersistenceDelegate::PromoteTemporaryKeyPair() {
  // TODO(b/290068552): Implement this method.
  return true;
}

bool MacKeyPersistenceDelegate::DeleteKeyPair(KeyStorageType type) {
  // TODO(b/290068552): Implement this method.
  return true;
}

void MacKeyPersistenceDelegate::CleanupTemporaryKeyData() {
  client_->DeleteKey(SecureEnclaveClient::KeyType::kTemporary);
}

}  // namespace enterprise_connectors
