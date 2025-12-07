// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_upload_request.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"

namespace enterprise_connectors {

namespace {

// Creating the request object involves generating a signature which may be
// resource intensive. It is, therefore, on a background thread.
std::optional<const enterprise_management::DeviceManagementRequest>
BuildUploadPublicKeyRequest(scoped_refptr<SigningKeyPair> key_pair) {
  if (!key_pair) {
    return std::nullopt;
  }
  return KeyUploadRequest::BuildUploadPublicKeyRequest(*key_pair);
}

void RecordUploadCode(int status_code) {
  static constexpr char kUploadCodeHistogram[] =
      "Enterprise.DeviceTrust.SyncSigningKey.UploadCode";
  base::UmaHistogramSparse(kUploadCodeHistogram, status_code);
}

}  // namespace

KeyLoaderImpl::KeyLoaderImpl(
    std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
        management_delegate)
    : cloud_management_delegate_(std::move(management_delegate)) {
  CHECK(cloud_management_delegate_);
}

KeyLoaderImpl::~KeyLoaderImpl() = default;

void KeyLoaderImpl::LoadKey(LoadKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadPersistedKey),
      base::BindOnce(&KeyLoaderImpl::SynchronizePublicKey,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyLoaderImpl::SynchronizePublicKey(LoadKeyCallback callback,
                                         LoadedKey persisted_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!persisted_key.key_pair ||
      persisted_key.result != LoadPersistedKeyResult::kSuccess) {
    LogSynchronizationError(DTSynchronizationError::kMissingKeyPair);
    std::move(callback).Run(DTCLoadKeyResult(persisted_key.result));
    return;
  }

    CHECK(cloud_management_delegate_);
    if (!cloud_management_delegate_->GetDMToken().has_value() ||
        cloud_management_delegate_->GetDMToken().value().empty()) {
      LogSynchronizationError(DTSynchronizationError::kInvalidDmToken);
      std::move(callback).Run(
          DTCLoadKeyResult(std::move(persisted_key.key_pair)));
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&BuildUploadPublicKeyRequest, persisted_key.key_pair),
        base::BindOnce(&KeyLoaderImpl::OnUploadPublicKeyRequestCreated,
                       weak_factory_.GetWeakPtr(), persisted_key.key_pair,
                       std::move(callback)));
}

void KeyLoaderImpl::OnUploadPublicKeyRequestCreated(
    scoped_refptr<SigningKeyPair> key_pair,
    LoadKeyCallback callback,
    std::optional<const enterprise_management::DeviceManagementRequest>
        upload_request) {
  if (!upload_request.has_value()) {
    LogSynchronizationError(DTSynchronizationError::kCannotBuildRequest);
    std::move(callback).Run(DTCLoadKeyResult(std::move(key_pair)));
    return;
  }

  cloud_management_delegate_->UploadBrowserPublicKey(
      std::move(upload_request.value()),
      base::BindOnce(&KeyLoaderImpl::OnUploadPublicKeyCompleted,
                     weak_factory_.GetWeakPtr(), std::move(key_pair),
                     std::move(callback)));
}

void KeyLoaderImpl::OnUploadPublicKeyCompleted(
    scoped_refptr<SigningKeyPair> key_pair,
    LoadKeyCallback callback,
    const policy::DMServerJobResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordUploadCode(result.response_code);
  std::move(callback).Run(
      DTCLoadKeyResult(result.response_code, std::move(key_pair)));
}

}  // namespace enterprise_connectors
