// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/syslog_logging.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "crypto/unexportable_key.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

// Status of the network delegates upload key request.
enum class UploadKeyStatus {
  SUCCEEDED,
  FAILED,
  FAILED_MAX_RETRIES,
};

constexpr int kMaxDMTokenLength = 4096;

BPKUR::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return BPKUR::RSA_KEY;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return BPKUR::EC_KEY;
  }
}

}  // namespace

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
  if (dm_token.size() > kMaxDMTokenLength) {
    SYSLOG(ERROR) << "DMToken length out of bounds";
    return false;
  }

  if (!persistence_delegate_->CheckRotationPermissions()) {
    RecordRotationStatus(nonce,
                         RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS);
    return false;
  }

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
  if (!new_key_pair) {
    RecordRotationStatus(nonce,
                         RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not generate a "
                     "new signing key.";
    return false;
  }

  if (!persistence_delegate_->StoreKeyPair(new_trust_level,
                                           new_key_pair->GetWrappedKey())) {
    RecordRotationStatus(nonce, RotationStatus::FAILURE_CANNOT_STORE_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not write to "
                     "signing key storage.";
    return false;
  }

  enterprise_management::DeviceManagementRequest request;
  if (!BuildUploadPublicKeyRequest(
          new_trust_level, new_key_pair, nonce,
          request.mutable_browser_public_key_upload_request())) {
    RecordRotationStatus(nonce, RotationStatus::FAILURE_CANNOT_BUILD_REQUEST);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not build the "
                     "upload key request.";
    return false;
  }

  std::string request_str;
  request.SerializeToString(&request_str);

  auto rc = UploadKeyStatus::FAILED;

  // Any attempt to reuse a nonce will result in an INVALID_SIGNATURE error
  // being returned by the server.
  KeyNetworkDelegate::HttpResponseCode response_code =
      network_delegate_->SendPublicKeyToDmServerSync(dm_server_url, dm_token,
                                                     request_str);

  RecordUploadCode(nonce, response_code);

  int status_leading_digit = response_code / 100;
  if (status_leading_digit == 2) {
    // 2xx response codes are treated as success.
    rc = UploadKeyStatus::SUCCEEDED;
  } else if (status_leading_digit == 5) {
    // 5xx response codes are treated as retriable errors.
    rc = UploadKeyStatus::FAILED_MAX_RETRIES;
  }

  if (rc != UploadKeyStatus::SUCCEEDED) {
    // Unable to send to DM server, so restore the old key if there was one.
    bool able_to_restore = true;
    if (key_pair_ && key_pair_->key()) {
      able_to_restore = persistence_delegate_->StoreKeyPair(
          key_pair_->trust_level(), key_pair_->key()->GetWrappedKey());
    } else {
      // If there was no old key we clear the registry.
      able_to_restore = persistence_delegate_->StoreKeyPair(
          BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>());
    }

    RotationStatus status =
        able_to_restore
            ? ((rc == UploadKeyStatus::FAILED_MAX_RETRIES)
                   ? RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY)
            : ((rc == UploadKeyStatus::FAILED_MAX_RETRIES)
                   ? RotationStatus::
                         FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED);
    RecordRotationStatus(nonce, status);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not send public "
                     "key to DM server.";
    return false;
  }

  key_pair_ = std::make_unique<SigningKeyPair>(std::move(new_key_pair),
                                               new_trust_level);
  RecordRotationStatus(nonce, RotationStatus::SUCCESS);
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
  request->set_key_type(AlgorithmToType(new_key_pair->Algorithm()));

  return true;
}

}  // namespace enterprise_connectors
