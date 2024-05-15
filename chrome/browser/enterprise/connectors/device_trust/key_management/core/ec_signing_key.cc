// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "build/build_config.h"
#include "crypto/ec_signature_creator.h"

#if BUILDFLAG(IS_MAC)
#include "base/notreached.h"
#endif  // BUILDFLAG(IS_MAC)

namespace enterprise_connectors {

namespace {

// An implementation of crypto::UnexportableSigningKey that is backed by an
// instance of crypto::ECPrivateKey.
class ECSigningKey : public crypto::UnexportableSigningKey {
 public:
  ECSigningKey();
  explicit ECSigningKey(base::span<const uint8_t> wrapped);
  ~ECSigningKey() override;

  // crypto::UnexportableSigningKey:
  crypto::SignatureVerifier::SignatureAlgorithm Algorithm() const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  std::vector<uint8_t> GetWrappedKey() const override;
  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override;

#if BUILDFLAG(IS_MAC)
  SecKeyRef GetSecKeyRef() const override;
#endif  // BUILDFLAG(IS_MAC)

 private:
  std::unique_ptr<crypto::ECPrivateKey> key_;
};

ECSigningKey::ECSigningKey() {
  key_ = crypto::ECPrivateKey::Create();
  DCHECK(key_);
}

ECSigningKey::ECSigningKey(base::span<const uint8_t> wrapped) {
  key_ = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(wrapped);
  DCHECK(key_);
}

ECSigningKey::~ECSigningKey() = default;

crypto::SignatureVerifier::SignatureAlgorithm ECSigningKey::Algorithm() const {
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

std::vector<uint8_t> ECSigningKey::GetSubjectPublicKeyInfo() const {
  std::vector<uint8_t> pubkey;
  bool ok = key_->ExportPublicKey(&pubkey);
  DCHECK(ok);
  return pubkey;
}

std::vector<uint8_t> ECSigningKey::GetWrappedKey() const {
  std::vector<uint8_t> wrapped;
  bool ok = key_->ExportPrivateKey(&wrapped);
  DCHECK(ok);
  return wrapped;
}

std::optional<std::vector<uint8_t>> ECSigningKey::SignSlowly(
    base::span<const uint8_t> data) {
  std::vector<uint8_t> signature;
  auto signer = crypto::ECSignatureCreator::Create(key_.get());
  DCHECK(signer);
  bool ok = signer->Sign(data, &signature);
  DCHECK(ok);
  return signature;
}

#if BUILDFLAG(IS_MAC)
SecKeyRef ECSigningKey::GetSecKeyRef() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

ECSigningKeyProvider::ECSigningKeyProvider() = default;
ECSigningKeyProvider::~ECSigningKeyProvider() = default;

std::optional<crypto::SignatureVerifier::SignatureAlgorithm>
ECSigningKeyProvider::SelectAlgorithm(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  for (auto algo : acceptable_algorithms) {
    if (algo == crypto::SignatureVerifier::ECDSA_SHA256)
      return crypto::SignatureVerifier::ECDSA_SHA256;
  }

  return std::nullopt;
}

std::unique_ptr<crypto::UnexportableSigningKey>
ECSigningKeyProvider::GenerateSigningKeySlowly(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  auto algo = SelectAlgorithm(acceptable_algorithms);
  if (!algo)
    return nullptr;

  DCHECK_EQ(crypto::SignatureVerifier::ECDSA_SHA256, *algo);
  return std::make_unique<ECSigningKey>();
}

std::unique_ptr<crypto::UnexportableSigningKey>
ECSigningKeyProvider::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return std::make_unique<ECSigningKey>(wrapped_key);
}

bool ECSigningKeyProvider::DeleteSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  // Software keys are stateless.
  return true;
}

}  // namespace enterprise_connectors
