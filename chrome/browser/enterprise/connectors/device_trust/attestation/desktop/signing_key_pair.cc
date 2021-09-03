// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include <stdint.h>

#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/ec_signing_key.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

absl::optional<std::vector<uint8_t>>& GetForcedTpmKeyStorage() {
  static base::NoDestructor<absl::optional<std::vector<uint8_t>>> storage;
  return *storage;
}

}  // namespace

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::Create() {
  auto key_pair = CreatePlatformKeyPair();
  if (key_pair)
    key_pair->Init();

  return key_pair;
}

// static
void SigningKeyPair::SetTpmKeyWrappedForTesting(
    const std::vector<uint8_t>& wrapped) {
  GetForcedTpmKeyStorage().emplace(wrapped);
}

// static
void SigningKeyPair::ClearTpmKeyWrappedForTesting() {
  GetForcedTpmKeyStorage().reset();
}

SigningKeyPair::SigningKeyPair() = default;

SigningKeyPair::~SigningKeyPair() = default;

void SigningKeyPair::Init() {
  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  std::vector<uint8_t> wrapped;

  auto& forced_tpm_key_wrapped = GetForcedTpmKeyStorage();
  if (forced_tpm_key_wrapped) {
    // Test hook to make use of ScopedMockUnexportableKeyProvider with the
    // appropriate algorithm.
    trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
    wrapped = forced_tpm_key_wrapped.value();
  } else {
    std::tie(trust_level, wrapped) = LoadKeyPair();
  }

  if (wrapped.empty()) {
    // No persisted key pair with a known trust level found.  This is not an
    // error, it could be that no key has been created yet.
    return;
  }

  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_TPM_KEY: {
      auto provider = crypto::GetUnexportableKeyProvider();
      if (provider) {
        key_pair_ = provider->FromWrappedSigningKeySlowly(wrapped);
      }
      break;
    }
    case BPKUR::CHROME_BROWSER_OS_KEY: {
      ECSigningKeyProvider provider;
      key_pair_ = provider.FromWrappedSigningKeySlowly(wrapped);
      break;
    }
    case BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED:
      NOTREACHED();
      return;
  }

  if (key_pair_)
    trust_level_ = trust_level;
}

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

bool SigningKeyPair::RotateWithAdminRights(const std::string& dm_token) {
  // Create a new key pair.  First try creating a TPM-backed key.  If that does
  // not work, try a less secure type.
  KeyTrustLevel new_trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  auto acceptable_algorithms = {
      crypto::SignatureVerifier::ECDSA_SHA256,
      crypto::SignatureVerifier::RSA_PKCS1_SHA256,
  };

  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      GetTpmBackedKeyProvider();
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

  if (!StoreKeyPair(new_trust_level, new_key_pair->GetWrappedKey()))
    return false;

  // TODO(b/195447899): send pubkey and signature to DM server.  If this fails
  // restore the old key and return false:
  //   StoreKeyPair(trust_level_, key_pair_->GetWrappedKey())
  //   return false;

  key_pair_ = std::move(new_key_pair);
  trust_level_ = new_trust_level;

  return true;
}

// Derived classes are expected to provide an implementation if possible.
std::unique_ptr<crypto::UnexportableKeyProvider>
SigningKeyPair::GetTpmBackedKeyProvider() {
  return nullptr;
}

}  // namespace enterprise_connectors
