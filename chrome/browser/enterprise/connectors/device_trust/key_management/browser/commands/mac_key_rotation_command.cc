// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

// Allows the key rotation manager to be released in the correct worker thread.
void OnBackgroundTearDown(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    base::OnceCallback<void(KeyRotationResult)> result_callback,
    KeyRotationResult result) {
  std::move(result_callback).Run(result);
}

// Runs on the thread pool.
base::expected<const enterprise_management::DeviceManagementRequest,
               KeyRotationResult>
StartCreatingKey() {
  std::unique_ptr<KeyRotationManager> key_rotation_manager =
      KeyRotationManager::Create();
  CHECK(key_rotation_manager);
  auto* key_rotation_manager_ptr = key_rotation_manager.get();

  // key_rotation_manager creates and returns the upload key request and or
  // KeyRotationResult in case of error.
  return key_rotation_manager_ptr->CreateUploadKeyRequest();
}

void FinishCreatingKey(
    base::OnceCallback<void(KeyRotationResult)> on_key_rotated,
    KeyNetworkDelegate::HttpResponseCode response_code) {
  std::unique_ptr<KeyRotationManager> key_rotation_manager =
      KeyRotationManager::Create();
  CHECK(key_rotation_manager);
  auto* key_rotation_manager_ptr = key_rotation_manager.get();

  key_rotation_manager_ptr->OnDmServerResponse(
      /*old_key_pair*/ nullptr,
      base::BindOnce(&OnBackgroundTearDown, std::move(key_rotation_manager),
                     std::move(on_key_rotated)),
      response_code);
}
}  // namespace

MacKeyRotationCommand::MacKeyRotationCommand(
    std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
        cloud_management_delegate)
    : cloud_management_delegate_(std::move(cloud_management_delegate)),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      client_(SecureEnclaveClient::Create()) {
  CHECK(cloud_management_delegate_);
  CHECK(client_);
}

MacKeyRotationCommand::~MacKeyRotationCommand() = default;

bool MacKeyRotationCommand::IsDmTokenValid() {
  CHECK(cloud_management_delegate_);
  if (!cloud_management_delegate_->GetDMToken().has_value() ||
      cloud_management_delegate_->GetDMToken().value().empty()) {
    SYSLOG(ERROR) << "DMToken empty";
    return false;
  }

  if (cloud_management_delegate_->GetDMToken().value().size() >
      KeyRotationManager::kMaxDMTokenLength) {
    SYSLOG(ERROR) << "DMToken length out of bounds";
    return false;
  }

  return true;
}

void MacKeyRotationCommand::UploadPublicKeyToDmServer(
    base::expected<const enterprise_management::DeviceManagementRequest,
                   KeyRotationResult> request) {
  CHECK(cloud_management_delegate_);

  if (request.has_value()) {
    cloud_management_delegate_->UploadBrowserPublicKey(
        std::move(request.value()),
        base::BindOnce(&MacKeyRotationCommand::OnUploadingPublicKeyCompleted,
                       weak_factory_.GetWeakPtr()));
  } else {
    OnKeyRotated(request.error());
  }
}

void MacKeyRotationCommand::OnUploadingPublicKeyCompleted(
    policy::DMServerJobResult result) {
  auto on_key_rotated = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &MacKeyRotationCommand::OnKeyRotated, weak_factory_.GetWeakPtr()));

  // Kicks off the key cleanup process in a worker thread.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FinishCreatingKey, std::move(on_key_rotated),
                                result.response_code));
}

void MacKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Parallel usage of command objects is not supported.
  CHECK(!pending_callback_);

  if (!client_->VerifySecureEnclaveSupported()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. The secure enclave is "
                     "not supported.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED_OS_RESTRICTION);
    return;
  }

  pending_callback_ = std::move(callback);

  timeout_timer_.Start(
      FROM_HERE, timeouts::kProcessWaitTimeout,
      base::BindOnce(&MacKeyRotationCommand::OnKeyRotationTimeout,
                     weak_factory_.GetWeakPtr()));

  auto on_key_rotated_callback =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &MacKeyRotationCommand::OnKeyRotated, weak_factory_.GetWeakPtr()));

  if (!IsDmTokenValid()) {
    RecordRotationStatus(/*is_rotation=*/false,
                         RotationStatus::FAILURE_INVALID_DMTOKEN);
    OnKeyRotated(KeyRotationResult::kFailed);
    return;
  }

  // Kicks off the key creation process in a worker thread.
  // The worker thread creates the key upload request and then on the main
  // thread, we upload the returned request to the DM Server.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&StartCreatingKey),
      base::BindOnce(&MacKeyRotationCommand::UploadPublicKeyToDmServer,
                     weak_factory_.GetWeakPtr()));
}

void MacKeyRotationCommand::OnKeyRotated(KeyRotationResult result) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  if (!pending_callback_) {
    // The callback may have already run in timeout cases.
    return;
  }
  timeout_timer_.Stop();

  auto response_status = KeyRotationCommand::Status::FAILED;
  switch (result) {
    case KeyRotationResult::kSucceeded:
      response_status = KeyRotationCommand::Status::SUCCEEDED;
      break;
    case KeyRotationResult::kFailed:
      SYSLOG(ERROR) << "Device trust key rotation failed.";
      response_status = KeyRotationCommand::Status::FAILED;
      break;
    case KeyRotationResult::kInsufficientPermissions:
      SYSLOG(ERROR) << "Device trust key rotation failed. The browser is "
                       "missing permissions.";
      response_status = KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS;
      break;
    case KeyRotationResult::kFailedKeyConflict:
      SYSLOG(ERROR) << "Device trust key rotation failed. Confict with the key "
                       "that exists on the server.";
      response_status = KeyRotationCommand::Status::FAILED_KEY_CONFLICT;
      break;
  }

  std::move(pending_callback_).Run(response_status);
}

void MacKeyRotationCommand::OnKeyRotationTimeout() {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // A callback should still be available to be run.
  if (!pending_callback_) {
    // The callback may have already run in timeout cases.
    return;
  }

  SYSLOG(ERROR) << "Device trust key rotation timed out.";
  std::move(pending_callback_).Run(KeyRotationCommand::Status::TIMED_OUT);
}

}  // namespace enterprise_connectors
