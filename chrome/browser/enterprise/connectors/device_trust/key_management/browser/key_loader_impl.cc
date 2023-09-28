// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_upload_request.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace enterprise_connectors {

namespace {

// Creating the request object involves generating a signature which may be
// resource intensive. It is, therefore, on a background thread.
absl::optional<const KeyUploadRequest> CreateRequest(
    const GURL& dm_server_url,
    const std::string& dm_token,
    scoped_refptr<SigningKeyPair> key_pair) {
  if (!key_pair) {
    return absl::nullopt;
  }
  return KeyUploadRequest::Create(dm_server_url, dm_token, *key_pair);
}

}  // namespace

KeyLoaderImpl::KeyLoaderImpl(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    std::unique_ptr<KeyNetworkDelegate> network_delegate)
    : dm_token_storage_(dm_token_storage),
      device_management_service_(device_management_service),
      network_delegate_(std::move(network_delegate)) {
  DCHECK(dm_token_storage_);
  DCHECK(device_management_service_);
  DCHECK(network_delegate_);
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

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  if (!dm_token.is_valid()) {
    LogSynchronizationError(DTSynchronizationError::kInvalidDmToken);
    std::move(callback).Run(
        DTCLoadKeyResult(std::move(persisted_key.key_pair)));
    return;
  }

  auto dm_server_url = GetUploadBrowserPublicKeyUrl(
      dm_token_storage_->RetrieveClientId(), dm_token.value(),
      device_management_service_);
  if (!dm_server_url) {
    LogSynchronizationError(DTSynchronizationError::kInvalidServerUrl);
    std::move(callback).Run(
        DTCLoadKeyResult(std::move(persisted_key.key_pair)));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CreateRequest, GURL(dm_server_url.value()),
                     dm_token.value(), persisted_key.key_pair),
      base::BindOnce(&KeyLoaderImpl::OnKeyUploadRequestCreated,
                     weak_factory_.GetWeakPtr(), persisted_key.key_pair,
                     std::move(callback)));
}

void KeyLoaderImpl::OnKeyUploadRequestCreated(
    scoped_refptr<SigningKeyPair> key_pair,
    LoadKeyCallback callback,
    absl::optional<const KeyUploadRequest> upload_request) {
  if (!upload_request) {
    LogSynchronizationError(DTSynchronizationError::kCannotBuildRequest);
    std::move(callback).Run(DTCLoadKeyResult(std::move(key_pair)));
    return;
  }

  network_delegate_->SendPublicKeyToDmServer(
      upload_request->dm_server_url(), upload_request->dm_token(),
      upload_request->request_body(),
      base::BindOnce(&KeyLoaderImpl::OnKeyUploadCompleted,
                     weak_factory_.GetWeakPtr(), std::move(key_pair),
                     std::move(callback)));
}

void KeyLoaderImpl::OnKeyUploadCompleted(
    scoped_refptr<enterprise_connectors::SigningKeyPair> key_pair,
    LoadKeyCallback callback,
    int status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kUploadCodeHistogram[] =
      "Enterprise.DeviceTrust.SyncSigningKey.UploadCode";
  base::UmaHistogramSparse(kUploadCodeHistogram, status_code);

  std::move(callback).Run(DTCLoadKeyResult(status_code, std::move(key_pair)));
}

}  // namespace enterprise_connectors
