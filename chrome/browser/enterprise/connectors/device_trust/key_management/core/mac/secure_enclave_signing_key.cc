// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_signing_key.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

// Returns the result of the comparison of the key `label` and the
// `wrapped_key_label`.
bool CheckEqual(const std::string& label,
                base::span<const uint8_t> wrapped_key_label) {
  auto label_span = base::as_bytes(base::make_span(label));
  return std::equal(wrapped_key_label.begin(), wrapped_key_label.end(),
                    label_span.begin(), label_span.end());
}

// Returns the key type from the `wrapped_key_label` if the `wrapped_key_label`
// matches any of the supported key labels. Otherwise a nullptr is returned.
absl::optional<SecureEnclaveClient::KeyType> GetTypeFromWrappedKey(
    base::span<const uint8_t> wrapped_key_label) {
  if (CheckEqual(constants::kDeviceTrustSigningKeyLabel, wrapped_key_label)) {
    return SecureEnclaveClient::KeyType::kPermanent;
  }

  if (CheckEqual(constants::kTemporaryDeviceTrustSigningKeyLabel,
                 wrapped_key_label)) {
    return SecureEnclaveClient::KeyType::kTemporary;
  }

  NOTREACHED();
  return absl::nullopt;
}

// An implementation of crypto::UnexportableSigningKey.
class SecureEnclaveSigningKey : public crypto::UnexportableSigningKey {
 public:
  SecureEnclaveSigningKey(base::ScopedCFTypeRef<SecKeyRef> key,
                          std::unique_ptr<SecureEnclaveClient> client,
                          SecureEnclaveClient::KeyType type);
  ~SecureEnclaveSigningKey() override;

  // crypto::UnexportableSigningKey:
  crypto::SignatureVerifier::SignatureAlgorithm Algorithm() const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  std::vector<uint8_t> GetWrappedKey() const override;
  absl::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override;

 private:
  base::ScopedCFTypeRef<SecKeyRef> key_;
  std::unique_ptr<SecureEnclaveClient> client_;
  SecureEnclaveClient::KeyType key_type_;
};

SecureEnclaveSigningKey::SecureEnclaveSigningKey(
    base::ScopedCFTypeRef<SecKeyRef> key,
    std::unique_ptr<SecureEnclaveClient> client,
    SecureEnclaveClient::KeyType type)
    : key_(std::move(key)), client_(std::move(client)), key_type_(type) {
  DCHECK(key_);
  DCHECK(client_);
}

SecureEnclaveSigningKey::~SecureEnclaveSigningKey() = default;

crypto::SignatureVerifier::SignatureAlgorithm
SecureEnclaveSigningKey::Algorithm() const {
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

std::vector<uint8_t> SecureEnclaveSigningKey::GetSubjectPublicKeyInfo() const {
  std::vector<uint8_t> pubkey;
  client_->ExportPublicKey(key_, pubkey);
  return pubkey;
}

std::vector<uint8_t> SecureEnclaveSigningKey::GetWrappedKey() const {
  std::vector<uint8_t> wrapped;
  client_->GetStoredKeyLabel(key_type_, wrapped);
  return wrapped;
}

absl::optional<std::vector<uint8_t>> SecureEnclaveSigningKey::SignSlowly(
    base::span<const uint8_t> data) {
  std::vector<uint8_t> signature;
  if (!client_->SignDataWithKey(key_, data, signature)) {
    return absl::nullopt;
  }

  return signature;
}

}  // namespace

SecureEnclaveSigningKeyProvider::SecureEnclaveSigningKeyProvider(
    SecureEnclaveClient::KeyType type)
    : provider_key_type_(type) {}
SecureEnclaveSigningKeyProvider::~SecureEnclaveSigningKeyProvider() = default;

absl::optional<crypto::SignatureVerifier::SignatureAlgorithm>
SecureEnclaveSigningKeyProvider::SelectAlgorithm(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  const auto kAlgorithm = crypto::SignatureVerifier::ECDSA_SHA256;
  if (base::Contains(acceptable_algorithms, kAlgorithm))
    return kAlgorithm;

  return absl::nullopt;
}

std::unique_ptr<crypto::UnexportableSigningKey>
SecureEnclaveSigningKeyProvider::GenerateSigningKeySlowly(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  if (provider_key_type_ != SecureEnclaveClient::KeyType::kTemporary)
    return nullptr;

  auto algo = SelectAlgorithm(acceptable_algorithms);
  if (!algo)
    return nullptr;

  DCHECK_EQ(crypto::SignatureVerifier::ECDSA_SHA256, *algo);

  auto client = SecureEnclaveClient::Create();
  auto key = client->CreateTemporaryKey();
  if (!key)
    return nullptr;

  return std::make_unique<SecureEnclaveSigningKey>(
      std::move(key), std::move(client), provider_key_type_);
}

std::unique_ptr<crypto::UnexportableSigningKey>
SecureEnclaveSigningKeyProvider::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key_label) {
  // Verifying wrapped key label matches the provider key type.
  if (provider_key_type_ != GetTypeFromWrappedKey(wrapped_key_label))
    return nullptr;

  auto client = SecureEnclaveClient::Create();
  auto key = client->CopyStoredKey(provider_key_type_);
  if (!key)
    return nullptr;

  return std::make_unique<SecureEnclaveSigningKey>(
      std::move(key), std::move(client), provider_key_type_);
}

}  // namespace enterprise_connectors
