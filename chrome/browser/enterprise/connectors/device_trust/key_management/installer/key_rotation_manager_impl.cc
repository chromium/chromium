// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "crypto/unexportable_key.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

KeyRotationManagerImpl::KeyRotationManagerImpl(
    std::unique_ptr<KeyNetworkDelegate> network_delegate,
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate)
    : network_delegate_(std::move(network_delegate)),
      persistence_delegate_(std::move(persistence_delegate)) {
  DCHECK(network_delegate_);
  DCHECK(persistence_delegate_);

  key_pair_ = SigningKeyPair::Create(persistence_delegate_.get());
}

KeyRotationManagerImpl::~KeyRotationManagerImpl() = default;

bool KeyRotationManagerImpl::RotateWithAdminRights(const GURL& dm_server_url,
                                                   const std::string& dm_token,
                                                   const std::string& nonce) {
  // Create a new key pair.  First try creating a TPM-backed key.  If that does
  // not work, try a less secure type.
  KeyTrustLevel new_trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  auto acceptable_algorithms = {
      crypto::SignatureVerifier::ECDSA_SHA256,
      crypto::SignatureVerifier::RSA_PKCS1_SHA256,
  };

  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      persistence_delegate_->GetTpmBackedKeyProvider();
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

  const net::BackoffEntry::Policy kBackoffPolicy{
      .num_errors_to_ignore = 0,
      .initial_delay_ms = 1000,
      .multiply_factor = 2.0,
      .jitter_factor = 0.1,
      .maximum_backoff_ms = 5 * 60 * 1000,  // 5 min.
      .entry_lifetime_ms = -1,
      .always_use_initial_delay = false};

  const int kMaxRetryCount = 10;
  auto rc = BPKUP::UNDEFINED;
  net::BackoffEntry boe(&kBackoffPolicy);
  for (int i = 0;
       rc == BPKUP::UNDEFINED && boe.failure_count() < kMaxRetryCount; ++i) {
    // Wait before trying to send again, if needed.  This will not block on
    // the first request.
    if (boe.ShouldRejectRequest())
      base::PlatformThread::Sleep(boe.GetTimeUntilRelease());

    // Any attempt to reuse a nonce will result in an INVALID_SIGNATURE error
    // being returned by the server.  This will cause the loop to break early.
    std::string response_str = network_delegate_->SendPublicKeyToDmServerSync(
        dm_server_url, dm_token, request_str);
    enterprise_management::DeviceManagementResponse response;
    rc = (!response_str.empty() && response.ParseFromString(response_str) &&
          response.has_browser_public_key_upload_response() &&
          response.browser_public_key_upload_response().has_response_code())
             ? response.browser_public_key_upload_response().response_code()
             : BPKUP::UNDEFINED;
    boe.InformOfRequest(rc == BPKUP::SUCCESS);
  }

  if (rc != BPKUP::SUCCESS) {
    // Unable to send to DM server, so restore the old key if there was one.
    if (key_pair_ && key_pair_->key()) {
      persistence_delegate_->StoreKeyPair(key_pair_->trust_level(),
                                          key_pair_->key()->GetWrappedKey());
    }
    return false;
  }

  key_pair_.emplace(std::move(new_key_pair), new_trust_level);

  return true;
}

bool KeyRotationManagerImpl::BuildUploadPublicKeyRequest(
    KeyTrustLevel new_trust_level,
    const std::unique_ptr<crypto::UnexportableSigningKey>& new_key_pair,
    const std::string& nonce,
    enterprise_management::BrowserPublicKeyUploadRequest* request) {
  std::vector<uint8_t> pubkey = new_key_pair->GetSubjectPublicKeyInfo();

  // Build the buffer to sign.  It consists of the public key of the new key
  // pair followed by the nonce.  The nonce vector may be empty.
  std::vector<uint8_t> buffer = pubkey;
  buffer.insert(buffer.end(), nonce.begin(), nonce.end());

  // If there is an existing key and the nonce is not empty, sign the new
  // pubkey with it.  Otherwise sign it with the new key itself (i.e. the
  // public key is self-signed).  This is done to handle the case of a device
  // that is enabled for device trust and then un-enrolled server side.  When
  // the user re-enrolls this device, the first key rotation attempt will use
  // an empty nonce to signal this is the first public key being uploaded to
  // DM server.  DM server expects the public key to be self signed.
  absl::optional<std::vector<uint8_t>> signature =
      key_pair_ && key_pair_->key() && !nonce.empty()
          ? key_pair_->key()->SignSlowly(buffer)
          : new_key_pair->SignSlowly(buffer);
  if (!signature.has_value())
    return false;

  request->set_public_key(pubkey.data(), pubkey.size());
  request->set_signature(signature->data(), signature->size());
  request->set_key_trust_level(new_trust_level);

  return true;
}

}  // namespace enterprise_connectors
