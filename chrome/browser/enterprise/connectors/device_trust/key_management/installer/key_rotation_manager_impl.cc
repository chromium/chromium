// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/syslog_logging.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_upload_request.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

bool IsValidKey(const SigningKeyPair* key_pair) {
  return key_pair && !key_pair->is_empty();
}

}  // namespace

KeyRotationManagerImpl::KeyRotationManagerImpl(
    std::unique_ptr<KeyNetworkDelegate> network_delegate,
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate)
    : network_delegate_(std::move(network_delegate)),
      persistence_delegate_(std::move(persistence_delegate)) {
  CHECK(network_delegate_);
  CHECK(persistence_delegate_);
}

KeyRotationManagerImpl::KeyRotationManagerImpl(
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate)
    : persistence_delegate_(std::move(persistence_delegate)) {
  CHECK(IsDTCKeyRotationUploadedBySharedAPI());
  CHECK(persistence_delegate_);
}

KeyRotationManagerImpl::~KeyRotationManagerImpl() = default;

base::expected<const enterprise_management::DeviceManagementRequest,
               KeyRotationResult>
KeyRotationManagerImpl::CreateUploadKeyRequest() {
  // Mac always permits rotation. We can skip checking for permissions.
  auto new_key_pair = persistence_delegate_->CreateKeyPair();
  if (!IsValidKey(new_key_pair.get())) {
    // TODO(b:254072094): We should rollback the storage when failing after
    // after the "Create key" step as, on Mac, storage is being updated in
    // this action.
    RecordRotationStatus(/*is_rotation=*/false,
                         RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY);
    SYSLOG(ERROR) << "Device trust key creation failed. Could not generate a "
                     "new signing key.";
    return base::unexpected(KeyRotationResult::kFailed);
  }

  auto request = KeyUploadRequest::BuildUploadPublicKeyRequest(*new_key_pair);

  if (!request.has_value()) {
    RecordRotationStatus(/*is_rotation=*/false,
                         RotationStatus::FAILURE_CANNOT_BUILD_REQUEST);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not build the "
                     "upload key request.";
    return base::unexpected(KeyRotationResult::kFailed);
  }

  if (!persistence_delegate_->StoreKeyPair(
          new_key_pair->trust_level(), new_key_pair->key()->GetWrappedKey())) {
    RecordRotationStatus(/*is_rotation=*/false,
                         RotationStatus::FAILURE_CANNOT_STORE_KEY);
    SYSLOG(ERROR) << "Device trust key creation failed. Could not write to "
                     "signing key storage.";
    return base::unexpected(KeyRotationResult::kFailed);
  }
  return request.value();
}

void KeyRotationManagerImpl::Rotate(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const std::string& nonce,
    base::OnceCallback<void(KeyRotationResult)> result_callback) {
  // If an old key exists, then the `nonce` becomes a required parameter as
  // we're effectively going through a key rotation flow instead of key
  // creation.
  auto old_key_pair =
      persistence_delegate_->LoadKeyPair(KeyStorageType::kPermanent, nullptr);
  const bool is_rotation = IsValidKey(old_key_pair.get());
  if (is_rotation && nonce.empty()) {
    RecordRotationStatus(/*is_rotation=*/true,
                         RotationStatus::FAILURE_INVALID_ROTATION_PARAMS);
    SYSLOG(ERROR) << "Device trust key rotation failed. Missing a nonce.";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  // DM Server params are not expected when the feature is enabled.
  if (!dm_server_url.is_valid()) {
    RecordRotationStatus(is_rotation,
                         RotationStatus::FAILURE_INVALID_DMSERVER_URL);
    SYSLOG(ERROR) << "DMServer URL invalid";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  if (dm_token.size() > kMaxDMTokenLength) {
    RecordRotationStatus(is_rotation, RotationStatus::FAILURE_INVALID_DMTOKEN);
    SYSLOG(ERROR) << "DMToken length out of bounds";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  if (dm_token.empty()) {
    RecordRotationStatus(is_rotation, RotationStatus::FAILURE_INVALID_DMTOKEN);
    SYSLOG(ERROR) << "DMToken empty";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  if (!persistence_delegate_->CheckRotationPermissions()) {
    RecordRotationStatus(is_rotation,
                         RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS);
    SYSLOG(ERROR) << "Device trust key rotation failed. Incorrect permissions.";
    std::move(result_callback).Run(KeyRotationResult::kInsufficientPermissions);
    return;
  }

  auto new_key_pair = persistence_delegate_->CreateKeyPair();
  if (!IsValidKey(new_key_pair.get())) {
    // TODO(b:254072094): We should rollback the storage when failing after
    // after the "Create key" step as, on Mac, storage is being updated in
    // this action.
    RecordRotationStatus(is_rotation,
                         RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not generate a "
                     "new signing key.";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  // Create a rotation or creation upload request based on the current
  // parameters.
  std::optional<const KeyUploadRequest> upload_request =
      is_rotation
          ? KeyUploadRequest::Create(dm_server_url, dm_token, *new_key_pair,
                                     *old_key_pair, nonce)
          : KeyUploadRequest::Create(dm_server_url, dm_token, *new_key_pair);
  if (!upload_request) {
    RecordRotationStatus(is_rotation,
                         RotationStatus::FAILURE_CANNOT_BUILD_REQUEST);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not build the "
                     "upload key request.";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  if (!persistence_delegate_->StoreKeyPair(
          new_key_pair->trust_level(), new_key_pair->key()->GetWrappedKey())) {
    RecordRotationStatus(is_rotation, RotationStatus::FAILURE_CANNOT_STORE_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not write to "
                     "signing key storage.";
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  // Any attempt to reuse a nonce will result in an INVALID_SIGNATURE error
  // being returned by the server.
  auto upload_key_callback = base::BindOnce(
      &KeyRotationManagerImpl::OnDmServerResponse, weak_factory_.GetWeakPtr(),
      std::move(old_key_pair), std::move(result_callback));
  network_delegate_->SendPublicKeyToDmServer(
      upload_request->dm_server_url(), upload_request->dm_token(),
      upload_request->request_body(), std::move(upload_key_callback));
}

void KeyRotationManagerImpl::OnDmServerResponse(
    scoped_refptr<SigningKeyPair> old_key_pair,
    base::OnceCallback<void(KeyRotationResult)> result_callback,
    KeyNetworkDelegate::HttpResponseCode response_code) {
  const bool is_rotation = IsValidKey(old_key_pair.get());
  RecordUploadCode(is_rotation, response_code);
  auto upload_key_status = ParseUploadKeyStatus(response_code);
  if (upload_key_status != UploadKeyStatus::kSucceeded) {
    // Unable to send to DM server, so restore the old key if there was one.
    bool able_to_restore = true;
    if (is_rotation) {
      able_to_restore = persistence_delegate_->StoreKeyPair(
          old_key_pair->trust_level(), old_key_pair->key()->GetWrappedKey());
    } else {
      // If there was no old key we clear the registry.
      able_to_restore = persistence_delegate_->StoreKeyPair(
          BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>());
    }

    RotationStatus status =
        able_to_restore
            ? ((upload_key_status == UploadKeyStatus::kFailedRetryable)
                   ? RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY)
            : ((upload_key_status == UploadKeyStatus::kFailedRetryable)
                   ? RotationStatus::
                         FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED);
    RecordRotationStatus(is_rotation, status);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not send public "
                     "key to DM server. HTTP Status: "
                  << response_code;
    if (upload_key_status == UploadKeyStatus::kFailedKeyConflict) {
      std::move(result_callback).Run(KeyRotationResult::kFailedKeyConflict);
      return;
    }

    // TODO(b:254072094): We should call CleanupTemporaryKeyData when failing
    // with a successful restore.
    std::move(result_callback).Run(KeyRotationResult::kFailed);
    return;
  }

  persistence_delegate_->CleanupTemporaryKeyData();
  RecordRotationStatus(is_rotation, RotationStatus::SUCCESS);
  std::move(result_callback).Run(KeyRotationResult::kSucceeded);
}

}  // namespace enterprise_connectors
