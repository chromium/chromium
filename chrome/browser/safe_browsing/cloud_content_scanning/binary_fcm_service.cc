// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"

namespace safe_browsing {

namespace {

const char kBinaryFCMServiceAppId[] = "safe_browsing_fcm_service";
const char kBinaryFCMServiceSenderId[] = "465959725923";
const char kBinaryFCMServiceMessageKey[] = "proto";

}  // namespace

const char BinaryFCMService::kInvalidId[] = "";

// static
std::unique_ptr<BinaryFCMService> BinaryFCMService::Create(Profile* profile) {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  if (!gcm_profile_service)
    return nullptr;

  gcm::GCMDriver* gcm_driver = gcm_profile_service->driver();
  if (!gcm_driver)
    return nullptr;

  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile);
  if (!instance_id_profile_service)
    return nullptr;

  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id_profile_service->driver();
  if (!instance_id_driver)
    return nullptr;

  return std::make_unique<BinaryFCMService>(gcm_driver, instance_id_driver);
}

BinaryFCMService::BinaryFCMService(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      weakptr_factory_(this) {
  gcm_driver->AddAppHandler(kBinaryFCMServiceAppId, this);
}

BinaryFCMService::BinaryFCMService()
    : gcm_driver_(nullptr),
      instance_id_driver_(nullptr),
      weakptr_factory_(this) {}

BinaryFCMService::~BinaryFCMService() {
  if (gcm_driver_ != nullptr)
    gcm_driver_->RemoveAppHandler(kBinaryFCMServiceAppId);
}

void BinaryFCMService::GetInstanceID(GetInstanceIDCallback callback) {
  if (pending_unregistrations_count_ > 0) {
    QueueGetInstanceIDCallback(std::move(callback));
    return;
  }

  instance_id_driver_->GetInstanceID(kBinaryFCMServiceAppId)
      ->GetToken(
          kBinaryFCMServiceSenderId, instance_id::kGCMScope,
          /*time_to_live=*/base::TimeDelta(),
          /*flags=*/{},
          base::BindOnce(&BinaryFCMService::OnGetInstanceID,
                         weakptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BinaryFCMService::QueueGetInstanceIDCallback(
    GetInstanceIDCallback callback) {
  pending_token_calls_.push_back(
      base::BindOnce(&BinaryFCMService::GetInstanceID,
                     weakptr_factory_.GetWeakPtr(), std::move(callback)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BinaryFCMService::MaybeRunNextQueuedOperation,
                     weakptr_factory_.GetWeakPtr()),
      delay_between_pending_attempts_);
}

void BinaryFCMService::SetCallbackForToken(const std::string& token,
                                           OnMessageCallback callback) {
  message_token_map_[token] = std::move(callback);
}

void BinaryFCMService::ClearCallbackForToken(const std::string& token) {
  message_token_map_.erase(token);
}

void BinaryFCMService::UnregisterInstanceID(
    const std::string& instance_id,
    UnregisterInstanceIDCallback callback) {
  if (instance_id_caller_counts_.contains(instance_id)) {
    DCHECK_NE(0u, instance_id_caller_counts_[instance_id]);
    instance_id_caller_counts_[instance_id]--;
    if (instance_id_caller_counts_[instance_id] == 0) {
      UnregisterInstanceIDImpl(instance_id, std::move(callback));
      instance_id_caller_counts_.erase(instance_id);
      return;
    }
  }

  // `callback` should always run so that the final steps of a request always
  // run. This is especially important if this is called for an auth request
  // that shared `instance_id` with another request.
  std::move(callback).Run(false);
}

void BinaryFCMService::SetQueuedOperationDelayForTesting(
    base::TimeDelta delay) {
  delay_between_pending_attempts_ = delay;
}

void BinaryFCMService::OnGetInstanceID(GetInstanceIDCallback callback,
                                       const std::string& instance_id,
                                       instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::SUCCESS) {
    instance_id_caller_counts_[instance_id]++;
    std::move(callback).Run(instance_id);

    // If we have queued operations, we know there is no async operation
    // currently pending, so start running the next operation early.
    if (!pending_token_calls_.empty()) {
      MaybeRunNextQueuedOperation();
    }
  } else if (result == instance_id::InstanceID::ASYNC_OPERATION_PENDING) {
    QueueGetInstanceIDCallback(std::move(callback));
  } else {
    std::move(callback).Run(kInvalidId);
  }
}

void BinaryFCMService::MaybeRunNextQueuedOperation() {
  if (!pending_token_calls_.empty()) {
    base::OnceClosure pending_operation =
        std::move(pending_token_calls_.front());
    pending_token_calls_.pop_front();
    std::move(pending_operation).Run();

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BinaryFCMService::MaybeRunNextQueuedOperation,
                       weakptr_factory_.GetWeakPtr()),
        delay_between_pending_attempts_);
  }
}

void BinaryFCMService::ShutdownHandler() {
  gcm_driver_ = nullptr;
}

void BinaryFCMService::OnStoreReset() {}

void BinaryFCMService::OnMessage(const std::string& app_id,
                                 const gcm::IncomingMessage& message) {
  auto serialized_proto_iterator =
      message.data.find(kBinaryFCMServiceMessageKey);
  if (serialized_proto_iterator == message.data.end())
    return;

  std::string serialized_proto;
  bool parsed =
      base::Base64Decode(serialized_proto_iterator->second, &serialized_proto);
  if (!parsed)
    return;

  enterprise_connectors::ContentAnalysisResponse response;
  parsed = response.ParseFromString(serialized_proto);

  if (!parsed)
    return;

  auto callback_it = message_token_map_.find(response.request_token());
  bool has_valid_token = (callback_it != message_token_map_.end());
  if (!has_valid_token)
    return;

  callback_it->second.Run(std::move(response));
}

void BinaryFCMService::OnMessagesDeleted(const std::string& app_id) {}

void BinaryFCMService::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {}

void BinaryFCMService::OnSendAcknowledged(const std::string& app_id,
                                          const std::string& message_id) {}

bool BinaryFCMService::CanHandle(const std::string& app_id) const {
  return app_id == kBinaryFCMServiceAppId;
}

bool BinaryFCMService::Connected() {
  if (!gcm_driver_)
    return false;

  // It's possible for the service to not be started yet if this is the first
  // FCM feature of the profile, so in that case assume it will be available
  // soon.
  if (!gcm_driver_->IsStarted())
    return true;

  return gcm_driver_->IsConnected();
}

void BinaryFCMService::UnregisterInstanceIDImpl(
    const std::string& instance_id,
    UnregisterInstanceIDCallback callback) {
  ++pending_unregistrations_count_;
  instance_id_driver_->GetInstanceID(kBinaryFCMServiceAppId)
      ->DeleteToken(kBinaryFCMServiceSenderId, instance_id::kGCMScope,
                    base::BindOnce(&BinaryFCMService::OnInstanceIDUnregistered,
                                   weakptr_factory_.GetWeakPtr(), instance_id,
                                   std::move(callback)));
}

void BinaryFCMService::OnInstanceIDUnregistered(
    const std::string& token,
    UnregisterInstanceIDCallback callback,
    instance_id::InstanceID::Result result) {
  --pending_unregistrations_count_;
  switch (result) {
    case instance_id::InstanceID::Result::SUCCESS:
      std::move(callback).Run(true);
      break;
    case instance_id::InstanceID::Result::INVALID_PARAMETER:
    case instance_id::InstanceID::Result::DISABLED:
    case instance_id::InstanceID::Result::UNKNOWN_ERROR:
      std::move(callback).Run(false);
      break;
    case instance_id::InstanceID::Result::ASYNC_OPERATION_PENDING:
    case instance_id::InstanceID::Result::NETWORK_ERROR:
    case instance_id::InstanceID::Result::SERVER_ERROR:
      UnregisterInstanceID(token, std::move(callback));
      break;
  }
}

void BinaryFCMService::Shutdown() {
  for (const auto& id_and_count : instance_id_caller_counts_) {
    const std::string& instance_id = id_and_count.first;
    UnregisterInstanceIDImpl(instance_id, base::DoNothing());
  }

  instance_id_caller_counts_.clear();
}

}  // namespace safe_browsing
