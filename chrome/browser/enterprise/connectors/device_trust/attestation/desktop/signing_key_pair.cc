// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include <stdint.h>

#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
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
  return CreateWithDelegates(std::make_unique<PersistenceDelegate>(),
                             std::make_unique<NetworkDelegate>());
}

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::CreateWithDelegates(
    std::unique_ptr<PersistenceDelegate> persistence_delegate,
    std::unique_ptr<NetworkDelegate> network_delegate) {
  auto key_pair = base::WrapUnique(new SigningKeyPair(
      std::move(persistence_delegate), std::move(network_delegate)));
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

SigningKeyPair::SigningKeyPair(
    std::unique_ptr<PersistenceDelegate> persistence_delegate,
    std::unique_ptr<NetworkDelegate> network_delegate)
    : persistence_delegate_(std::move(persistence_delegate)),
      network_delegate_(std::move(network_delegate)) {
  DCHECK(persistence_delegate_);
  DCHECK(network_delegate_);
}

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
    std::tie(trust_level, wrapped) = persistence_delegate_->LoadKeyPair();
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
  // TODO: In followup CL, these will arguments to the function.
  std::string dm_server_url;
  std::string nonce;

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
    ECSigningKeyProvider ec_signing_provider;
    new_key_pair =
        ec_signing_provider.GenerateSigningKeySlowly(acceptable_algorithms);
  }
  if (!new_key_pair)
    return false;

  if (!persistence_delegate_->StoreKeyPair(new_trust_level,
                                           new_key_pair->GetWrappedKey())) {
    return false;
  }

  enterprise_management::DeviceManagementRequest request;
  if (!BuildUploadPublicKeyRequest(
          new_trust_level, new_key_pair, nonce,
          request.mutable_browser_public_key_upload_request())) {
    return false;
  }

  std::string request_str;
  request.SerializeToString(&request_str);
  std::string response_str = network_delegate_->SendPublicKeyToDmServerSync(
      dm_server_url, dm_token, request_str);
  enterprise_management::DeviceManagementResponse response;
  if (response_str.empty() || !response.ParseFromString(response_str) ||
      !response.has_browser_public_key_upload_response() ||
      !response.browser_public_key_upload_response().has_response_code() ||
      response.browser_public_key_upload_response().response_code() !=
          enterprise_management::BrowserPublicKeyUploadResponse::SUCCESS) {
    // Unable to send to DM server, so restore the old key if there was one.
    if (key_pair_) {
      persistence_delegate_->StoreKeyPair(trust_level_,
                                          key_pair_->GetWrappedKey());
    }
    return false;
  }

  key_pair_ = std::move(new_key_pair);
  trust_level_ = new_trust_level;

  return true;
}

bool SigningKeyPair::BuildUploadPublicKeyRequest(
    KeyTrustLevel new_trust_level,
    const std::unique_ptr<crypto::UnexportableSigningKey>& new_key_pair,
    const std::string& nonce,
    enterprise_management::BrowserPublicKeyUploadRequest* request) {
  std::vector<uint8_t> pubkey = new_key_pair->GetSubjectPublicKeyInfo();

  // Build the buffer to sign.  It consists of the public key of the new key
  // pair followed by the nonce.  The nonce vector may be empty.
  std::vector<uint8_t> buffer = pubkey;
  buffer.insert(buffer.end(), nonce.begin(), nonce.end());

  // If there is an existing key, sign the new pubkey with it.  Otherwise sign
  // it with the new key itself (i.e. the public key is self-signed).
  absl::optional<std::vector<uint8_t>> signature =
      key_pair_ ? key_pair_->SignSlowly(buffer)
                : new_key_pair->SignSlowly(buffer);
  if (!signature.has_value())
    return false;

  request->set_public_key(pubkey.data(), pubkey.size());
  request->set_signature(signature->data(), signature->size());
  request->set_key_trust_level(new_trust_level);

  return true;
}

}  // namespace enterprise_connectors
