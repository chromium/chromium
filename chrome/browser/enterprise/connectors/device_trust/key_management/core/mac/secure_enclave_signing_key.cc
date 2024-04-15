// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_signing_key.h"

#include <Security/Security.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "crypto/signature_verifier.h"

namespace enterprise_connectors {

namespace {

// An implementation of crypto::UnexportableSigningKey.
class SecureEnclaveSigningKey : public crypto::UnexportableSigningKey {
 public:
  SecureEnclaveSigningKey(base::apple::ScopedCFTypeRef<SecKeyRef> key,
                          std::unique_ptr<SecureEnclaveClient> client,
                          SecureEnclaveClient::KeyType key_type);
  ~SecureEnclaveSigningKey() override;

  // crypto::UnexportableSigningKey:
  crypto::SignatureVerifier::SignatureAlgorithm Algorithm() const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  std::vector<uint8_t> GetWrappedKey() const override;
  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override;
  SecKeyRef GetSecKeyRef() const override;

 private:
  base::apple::ScopedCFTypeRef<SecKeyRef> key_;
  std::unique_ptr<SecureEnclaveClient> client_;
  SecureEnclaveClient::KeyType key_type_;
};

SecureEnclaveSigningKey::SecureEnclaveSigningKey(
    base::apple::ScopedCFTypeRef<SecKeyRef> key,
    std::unique_ptr<SecureEnclaveClient> client,
    SecureEnclaveClient::KeyType key_type)
    : key_(std::move(key)), client_(std::move(client)), key_type_(key_type) {
  CHECK(key_);
  CHECK(client_);
}

SecureEnclaveSigningKey::~SecureEnclaveSigningKey() = default;

crypto::SignatureVerifier::SignatureAlgorithm
SecureEnclaveSigningKey::Algorithm() const {
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

std::vector<uint8_t> SecureEnclaveSigningKey::GetSubjectPublicKeyInfo() const {
  std::vector<uint8_t> pubkey;

  OSStatus error;
  if (!client_->ExportPublicKey(key_.get(), pubkey, &error)) {
    RecordKeyOperationStatus(KeychainOperation::kExportPublicKey, key_type_,
                             error);
  }

  return pubkey;
}

std::vector<uint8_t> SecureEnclaveSigningKey::GetWrappedKey() const {
  std::vector<uint8_t> wrapped;
  if (key_) {
    auto label = SecureEnclaveClient::GetLabelFromKeyType(key_type_);
    wrapped.assign(label.begin(), label.end());
  }
  return wrapped;
}

std::optional<std::vector<uint8_t>> SecureEnclaveSigningKey::SignSlowly(
    base::span<const uint8_t> data) {
  std::vector<uint8_t> signature;
  OSStatus error;
  if (!client_->SignDataWithKey(key_.get(), data, signature, &error)) {
    RecordKeyOperationStatus(KeychainOperation::kSignPayload, key_type_, error);
    return std::nullopt;
  }

  return signature;
}

SecKeyRef SecureEnclaveSigningKey::GetSecKeyRef() const {
  return key_.get();
}

}  // namespace

SecureEnclaveSigningKeyProvider::SecureEnclaveSigningKeyProvider() = default;

SecureEnclaveSigningKeyProvider::~SecureEnclaveSigningKeyProvider() = default;

std::unique_ptr<crypto::UnexportableSigningKey>
SecureEnclaveSigningKeyProvider::GenerateSigningKeySlowly() {
  auto client = SecureEnclaveClient::Create();
  auto key = client->CreatePermanentKey();
  if (!key) {
    return nullptr;
  }

  return std::make_unique<SecureEnclaveSigningKey>(
      std::move(key), std::move(client),
      SecureEnclaveClient::KeyType::kPermanent);
}

std::unique_ptr<crypto::UnexportableSigningKey>
SecureEnclaveSigningKeyProvider::LoadStoredSigningKeySlowly(
    SecureEnclaveClient::KeyType key_type,
    OSStatus* error) {
  auto client = SecureEnclaveClient::Create();

  auto key = client->CopyStoredKey(key_type, error);
  if (!key) {
    return nullptr;
  }

  return std::make_unique<SecureEnclaveSigningKey>(std::move(key),
                                                   std::move(client), key_type);
}

}  // namespace enterprise_connectors
