// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kStableChannelHostName[] = "m.google.com";

bool ValidRotationCommand(const std::string& host_name) {
  return chrome::GetChannel() != version_info::Channel::STABLE ||
         host_name == kStableChannelHostName;
}

// Allows the key rotation maanger to be released in the correct worker thread.
void OnBackgroundTearDown(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    base::OnceCallback<void(KeyRotationManager::Result)> result_callback,
    KeyRotationManager::Result result) {
  std::move(result_callback).Run(result);
}

// Runs on the thread pool.
void StartRotation(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const std::string& nonce,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    base::OnceCallback<void(KeyRotationManager::Result)> result_callback) {
  DCHECK(pending_url_loader_factory);
  auto key_rotation_manager =
      KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
          network::SharedURLLoaderFactory::Create(
              std::move(pending_url_loader_factory))));
  DCHECK(key_rotation_manager);

  auto* key_rotation_manager_ptr = key_rotation_manager.get();
  key_rotation_manager_ptr->Rotate(
      dm_server_url, dm_token, nonce,
      base::BindOnce(&OnBackgroundTearDown, std::move(key_rotation_manager),
                     std::move(result_callback)));
}

}  // namespace

MacKeyRotationCommand::MacKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_prefs)
    : url_loader_factory_(std::move(url_loader_factory)),
      local_prefs_(local_prefs),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      client_(SecureEnclaveClient::Create()) {
  DCHECK(url_loader_factory_);
  DCHECK(client_);
}

MacKeyRotationCommand::~MacKeyRotationCommand() = default;

void MacKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Parallel usage of command objects is not supported.
  DCHECK(!pending_callback_);

  if (!client_->VerifySecureEnclaveSupported()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. The secure enclave is "
                     "not supported.";
    local_prefs_->SetBoolean(kDeviceTrustDisableKeyCreationPref, true);
    std::move(callback).Run(KeyRotationCommand::Status::FAILED_OS_RESTRICTION);
    return;
  }

  GURL dm_server_url(params.dm_server_url);
  if (!ValidRotationCommand(dm_server_url.host())) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. The server URL is invalid.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  pending_callback_ = std::move(callback);

  timeout_timer_.Start(
      FROM_HERE, timeouts::kProcessWaitTimeout,
      base::BindOnce(&MacKeyRotationCommand::OnKeyRotationTimeout,
                     weak_factory_.GetWeakPtr()));

  auto rotation_result_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&MacKeyRotationCommand::OnKeyRotated,
                                        weak_factory_.GetWeakPtr()));

  // Kicks off the key rotation process in a worker thread.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StartRotation, dm_server_url, params.dm_token,
                                params.nonce, url_loader_factory_->Clone(),
                                std::move(rotation_result_callback)));
}

void MacKeyRotationCommand::OnKeyRotated(KeyRotationManager::Result result) {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  if (!pending_callback_) {
    // The callback may have already run in timeout cases.
    return;
  }

  timeout_timer_.Stop();

  if (result == KeyRotationManager::Result::FAILED) {
    SYSLOG(ERROR) << "Device trust key rotation failed.";
    std::move(pending_callback_).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  if (result == KeyRotationManager::Result::FAILED_KEY_CONFLICT) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Conflict "
                     "with the key that exists on the server.";
    local_prefs_->SetBoolean(kDeviceTrustDisableKeyCreationPref, true);
    std::move(pending_callback_)
        .Run(KeyRotationCommand::Status::FAILED_KEY_CONFLICT);
    return;
  }

  std::move(pending_callback_).Run(KeyRotationCommand::Status::SUCCEEDED);
}

void MacKeyRotationCommand::OnKeyRotationTimeout() {
  // Used to ensure that this function is being called on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // A callback should still be available to be run.
  DCHECK(pending_callback_);

  SYSLOG(ERROR) << "Device trust key rotation timed out.";
  std::move(pending_callback_).Run(KeyRotationCommand::Status::TIMED_OUT);
}

}  // namespace enterprise_connectors
