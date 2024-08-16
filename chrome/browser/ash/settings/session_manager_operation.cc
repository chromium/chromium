// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/session_manager_operation.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/session_manager/policy_descriptor.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"

using ownership::OwnerKeyUtil;
using ownership::PublicKey;

namespace em = enterprise_management;

namespace ash {

namespace {

constexpr char kEmptyAccountId[] = "";

}  // namespace

using RetrievePolicyResponseType =
    SessionManagerClient::RetrievePolicyResponseType;

SessionManagerOperation::SessionManagerOperation(Callback callback)
    : callback_(std::move(callback)) {}

SessionManagerOperation::~SessionManagerOperation() {}

void SessionManagerOperation::Start(
    SessionManagerClient* session_manager_client,
    scoped_refptr<OwnerKeyUtil> owner_key_util,
    scoped_refptr<PublicKey> public_key) {
  session_manager_client_ = session_manager_client;
  owner_key_util_ = owner_key_util;
  public_key_ = public_key;
  Run();
}

void SessionManagerOperation::RestartLoad(bool key_changed) {
  if (key_changed)
    public_key_ = nullptr;

  if (!is_loading_)
    return;

  // Abort previous load operations.
  weak_factory_.InvalidateWeakPtrs();
  // Mark as not loading to start loading again.
  is_loading_ = false;
  StartLoading();
}

void SessionManagerOperation::StartLoading() {
  if (is_loading_)
    return;
  is_loading_ = true;
  EnsurePublicKey(
      base::BindOnce(&SessionManagerOperation::RetrieveDeviceSettings,
                     weak_factory_.GetWeakPtr()));
}

void SessionManagerOperation::LoadImmediately() {
  StorePublicKey(
      base::BindOnce(&SessionManagerOperation::BlockingRetrieveDeviceSettings,
                     weak_factory_.GetWeakPtr()),
      LoadPublicKey(owner_key_util_, public_key_));
}

void SessionManagerOperation::ReportResult(
    DeviceSettingsService::Status status) {
  std::move(callback_).Run(this, status);
}

void SessionManagerOperation::EnsurePublicKey(base::OnceClosure callback) {
  if (force_key_load_ || !public_key_ || public_key_->is_empty()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&SessionManagerOperation::LoadPublicKey, owner_key_util_,
                       force_key_load_ ? nullptr : public_key_),
        base::BindOnce(&SessionManagerOperation::StorePublicKey,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

// static
scoped_refptr<PublicKey> SessionManagerOperation::LoadPublicKey(
    scoped_refptr<OwnerKeyUtil> util,
    scoped_refptr<PublicKey> current_key) {
  // Keep already-existing public key.
  if (current_key && !current_key->is_empty()) {
    return current_key->clone();
  }

  scoped_refptr<PublicKey> public_key =
      base::MakeRefCounted<ownership::PublicKey>(
          /*is_persisted=*/false, /*data=*/std::vector<uint8_t>());

  if (util->IsPublicKeyPresent()) {
    public_key = util->ImportPublicKey();
    if (!public_key || public_key->is_empty())
      LOG(ERROR) << "Failed to load public owner key.";
  }

  return public_key;
}

void SessionManagerOperation::StorePublicKey(base::OnceClosure callback,
                                             scoped_refptr<PublicKey> new_key) {
  force_key_load_ = false;
  public_key_ = new_key;

  if (!public_key_ || public_key_->is_empty()) {
    ReportResult(DeviceSettingsService::STORE_KEY_UNAVAILABLE);
    return;
  }

  std::move(callback).Run();
}

void SessionManagerOperation::RetrieveDeviceSettings() {
  login_manager::PolicyDescriptor descriptor = ash::MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  session_manager_client()->RetrievePolicy(
      descriptor,
      base::BindOnce(&SessionManagerOperation::ValidateDeviceSettings,
                     weak_factory_.GetWeakPtr()));
}

void SessionManagerOperation::BlockingRetrieveDeviceSettings() {
  std::string policy_blob;
  login_manager::PolicyDescriptor descriptor = ash::MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE, kEmptyAccountId);
  RetrievePolicyResponseType response =
      session_manager_client()->BlockingRetrievePolicy(descriptor,
                                                       &policy_blob);
  ValidateDeviceSettings(response, policy_blob);
}

void SessionManagerOperation::ValidateDeviceSettings(
    RetrievePolicyResponseType response_type,
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    ReportResult(DeviceSettingsService::STORE_NO_POLICY);
    return;
  }

  std::unique_ptr<em::PolicyFetchResponse> policy =
      std::make_unique<em::PolicyFetchResponse>();
  if (!policy->ParseFromString(policy_blob) || !policy->IsInitialized()) {
    ReportResult(DeviceSettingsService::STORE_INVALID_POLICY);
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  std::unique_ptr<policy::DeviceCloudPolicyValidator> validator =
      std::make_unique<policy::DeviceCloudPolicyValidator>(
          std::move(policy), background_task_runner);

  // Policy auto-generated by session manager doesn't include a timestamp, so
  // the timestamp shouldn't be verified in that case. Note that the timestamp
  // is still verified during enrollment and when a new policy is fetched from
  // the server.
  //
  // The two *_NOT_REQUIRED options are necessary because both the DM token
  // and the device id are empty for a user logging in on an actual Chrome OS
  // device that is not enterprise-managed. Note for devs: The strings are not
  // empty when you test Chrome with target_os = "chromeos" on Linux!
  validator->ValidateAgainstCurrentPolicy(
      policy_data_.get(),
      policy::CloudPolicyValidatorBase::TIMESTAMP_NOT_VALIDATED,
      policy::CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED,
      policy::CloudPolicyValidatorBase::DEVICE_ID_NOT_REQUIRED);

  // We don't check the DMServer verification key below, because the signing
  // key is validated when it is installed.
  validator->ValidateSignature(public_key_->as_string());

  validator->ValidatePolicyType(policy::dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  if (force_immediate_load_) {
    validator->RunValidation();
    ReportValidatorStatus(validator.get());
  } else {
    policy::DeviceCloudPolicyValidator::StartValidation(
        std::move(validator),
        base::BindOnce(&SessionManagerOperation::ReportValidatorStatus,
                       weak_factory_.GetWeakPtr()));
  }
}

void SessionManagerOperation::ReportValidatorStatus(
    policy::DeviceCloudPolicyValidator* validator) {
  if (validator->success()) {
    policy_fetch_response_ = std::move(validator->policy());
    policy_data_ = std::move(validator->policy_data());
    device_settings_ = std::move(validator->payload());
    ReportResult(DeviceSettingsService::STORE_SUCCESS);
  } else {
    LOG(ERROR) << "Policy validation failed: " << validator->status() << " ("
               << policy::DeviceCloudPolicyValidator::StatusToString(
                      validator->status())
               << ")";
    ReportResult(DeviceSettingsService::STORE_VALIDATION_ERROR);
  }
}

LoadSettingsOperation::LoadSettingsOperation(bool force_key_load,
                                             bool force_immediate_load,
                                             Callback callback)
    : SessionManagerOperation(std::move(callback)) {
  force_key_load_ = force_key_load;
  force_immediate_load_ = force_immediate_load;
}

LoadSettingsOperation::~LoadSettingsOperation() {}

void LoadSettingsOperation::Run() {
  if (force_immediate_load_)
    LoadImmediately();
  else
    StartLoading();
}

StoreSettingsOperation::StoreSettingsOperation(
    Callback callback,
    std::unique_ptr<em::PolicyFetchResponse> policy_fetch_response)
    : SessionManagerOperation(std::move(callback)) {
  policy_fetch_response_ = std::move(policy_fetch_response);
  if (policy_fetch_response_->has_new_public_key())
    force_key_load_ = true;
}

StoreSettingsOperation::~StoreSettingsOperation() {}

void StoreSettingsOperation::Run() {
  session_manager_client()->StoreDevicePolicy(
      policy_fetch_response_->SerializeAsString(),
      base::BindOnce(&StoreSettingsOperation::HandleStoreResult,
                     weak_factory_.GetWeakPtr()));
}

void StoreSettingsOperation::HandleStoreResult(bool success) {
  if (!success)
    ReportResult(DeviceSettingsService::STORE_OPERATION_FAILED);
  else
    StartLoading();
}

}  // namespace ash
