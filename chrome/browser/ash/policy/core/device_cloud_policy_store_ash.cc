// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/device_policy_decoder.h"
#include "chrome/browser/ash/policy/dev_mode/dev_mode_policy_util.h"
#include "chrome/browser/ash/policy/value_validation/onc_device_policy_value_validator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace features {

BASE_FEATURE(kDeviceIdValidation,
             "DeviceIdValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace policy {

namespace {

const char kDMTokenCheckHistogram[] = "Enterprise.EnrolledPolicyHasDMToken";
const char kPolicyCheckHistogram[] = "Enterprise.EnrolledDevicePolicyPresent";

bool CanUseDeviceIdValidation() {
  if (!base::FeatureList::IsEnabled(features::kDeviceIdValidation)) {
    return false;
  }

  // The devices are storing the OS version in the local state at enrollment,
  // starting from version M122. For those devices the stats shows 100%
  // matching of device_id from policy with the value from install attributes.
  // Therefore we consider a low risk for them to enforce the device_id
  // validation now.
  auto* local_state = g_browser_process->local_state();
  return local_state &&
         !local_state->GetString(prefs::kEnrollmentVersionOS).empty();
}

}  // namespace

DeviceCloudPolicyStoreAsh::DeviceCloudPolicyStoreAsh(
    ash::DeviceSettingsService* device_settings_service,
    ash::InstallAttributes* install_attributes,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : device_settings_service_(device_settings_service),
      install_attributes_(install_attributes),
      background_task_runner_(background_task_runner) {
  device_settings_service_->AddObserver(this);
  device_settings_service_->SetDeviceMode(install_attributes_->GetMode());
}

DeviceCloudPolicyStoreAsh::~DeviceCloudPolicyStoreAsh() {
  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);
}

void DeviceCloudPolicyStoreAsh::Store(const em::PolicyFetchResponse& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The policy and the public key must have already been loaded by the device
  // settings service.
  DCHECK(is_initialized());

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  scoped_refptr<ownership::PublicKey> public_key(
      device_settings_service_->GetPublicKey());
  if (!install_attributes_->IsCloudManaged() ||
      !device_settings_service_->policy_data() || !public_key.get() ||
      public_key->is_empty()) {
    LOG(ERROR) << "Policy store failed, is_cloud_managed: "
               << install_attributes_->IsCloudManaged() << ", policy_data: "
               << (device_settings_service_->policy_data() != nullptr)
               << ", public_key: " << (public_key.get() != nullptr)
               << ", public_key_is_loaded: "
               << (public_key.get() ? !public_key->is_empty() : false);
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  std::unique_ptr<DeviceCloudPolicyValidator> validator(
      CreateValidator(policy));
  validator->ValidateSignatureAllowingRotation(
      public_key->as_string(), install_attributes_->GetDomain());
  validator->ValidateAgainstCurrentPolicy(
      device_settings_service_->policy_data(),
      CloudPolicyValidatorBase::TIMESTAMP_VALIDATED,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->RunValidation();
  OnPolicyToStoreValidated(validator.get());
}

void DeviceCloudPolicyStoreAsh::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  device_settings_service_->Load();
}

void DeviceCloudPolicyStoreAsh::InstallInitialPolicy(
    const em::PolicyFetchResponse& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  if (!install_attributes_->IsCloudManaged()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  std::unique_ptr<DeviceCloudPolicyValidator> validator(
      CreateValidator(policy));
  validator->ValidateInitialKey(install_attributes_->GetDomain());
  validator->ValidateDeviceId(install_attributes_->GetDeviceId(),
                              CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->RunValidation();
  OnPolicyToStoreValidated(validator.get());
}

void DeviceCloudPolicyStoreAsh::DeviceSettingsUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!weak_factory_.HasWeakPtrs())
    UpdateFromService();
}

void DeviceCloudPolicyStoreAsh::OnDeviceSettingsServiceShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_settings_service_->RemoveObserver(this);
  device_settings_service_ = nullptr;
}

std::unique_ptr<DeviceCloudPolicyValidator>
DeviceCloudPolicyStoreAsh::CreateValidator(
    const em::PolicyFetchResponse& policy) {
  auto validator = std::make_unique<DeviceCloudPolicyValidator>(
      std::make_unique<em::PolicyFetchResponse>(policy),
      background_task_runner_);
  if (CanUseDeviceIdValidation()) {
    validator->ValidateDeviceId(install_attributes_->GetDeviceId(),
                                CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  }

  // TODO(b:256551074): The domain validation is planned to be removed when we
  // confirm that the device_id validation works.
  validator->ValidateDomain(install_attributes_->GetDomain());
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  validator->ValidateValues(std::make_unique<ONCDevicePolicyValueValidator>());
  return validator;
}

void DeviceCloudPolicyStoreAsh::OnPolicyToStoreValidated(
    DeviceCloudPolicyValidator* validator) {
  validation_result_ = validator->GetValidationResult();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  if (GetDeviceBlockDevModePolicyValue(*(validator->payload())) &&
      !IsDeviceBlockDevModePolicyAllowed()) {
    LOG(ERROR) << "Rejected device policy: DeviceBlockDevmode not allowed";
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  device_settings_service_->Store(
      std::move(validator->policy()),
      base::BindOnce(&DeviceCloudPolicyStoreAsh::OnPolicyStored,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCloudPolicyStoreAsh::OnPolicyStored() {
  UpdateFromService();
}

void DeviceCloudPolicyStoreAsh::UpdateFromService() {
  if (!install_attributes_->IsEnterpriseManaged()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  CheckDMToken();
  UpdateStatusFromService();

  const ash::DeviceSettingsService::Status service_status =
      device_settings_service_->status();
  if (service_status == ash::DeviceSettingsService::STORE_SUCCESS) {
    auto new_policy_fetch_response =
        std::make_unique<em::PolicyFetchResponse>();
    auto new_policy = std::make_unique<em::PolicyData>();
    const em::PolicyFetchResponse* policy_fetch_response =
        device_settings_service_->policy_fetch_response();
    const em::PolicyData* policy_data = device_settings_service_->policy_data();
    if (policy_data) {
      DCHECK(policy_fetch_response);
      new_policy_fetch_response->MergeFrom(*policy_fetch_response);
      new_policy->MergeFrom(*policy_data);
    }
    SetPolicy(std::move(new_policy_fetch_response), std::move(new_policy));

    PolicyMap new_policy_map;
    if (is_managed()) {
      DecodeDevicePolicy(*device_settings_service_->device_settings(),
                         external_data_manager(), &new_policy_map);
    }
    policy_map_.Swap(&new_policy_map);

    scoped_refptr<ownership::PublicKey> key =
        device_settings_service_->GetPublicKey();
    policy_signature_public_key_ = key ? key->as_string() : std::string();

    NotifyStoreLoaded();
    return;
  }
  NotifyStoreError();
}

void DeviceCloudPolicyStoreAsh::UpdateStatusFromService() {
  switch (device_settings_service_->status()) {
    case ash::DeviceSettingsService::STORE_SUCCESS:
      status_ = STATUS_OK;
      return;
    case ash::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      status_ = STATUS_BAD_STATE;
      return;
    case ash::DeviceSettingsService::STORE_OPERATION_FAILED:
      status_ = STATUS_STORE_ERROR;
      return;
    case ash::DeviceSettingsService::STORE_NO_POLICY:
    case ash::DeviceSettingsService::STORE_INVALID_POLICY:
    case ash::DeviceSettingsService::STORE_VALIDATION_ERROR:
      status_ = STATUS_LOAD_ERROR;
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void DeviceCloudPolicyStoreAsh::CheckDMToken() {
  const ash::DeviceSettingsService::Status service_status =
      device_settings_service_->status();
  switch (service_status) {
    case ash::DeviceSettingsService::STORE_SUCCESS:
    case ash::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
    case ash::DeviceSettingsService::STORE_NO_POLICY:
    case ash::DeviceSettingsService::STORE_INVALID_POLICY:
    case ash::DeviceSettingsService::STORE_VALIDATION_ERROR:
      // Continue with the check below.
      break;
    case ash::DeviceSettingsService::STORE_OPERATION_FAILED:
      // Don't check for write errors or transient read errors.
      return;
  }

  if (dm_token_checked_) {
    return;
  }
  dm_token_checked_ = true;

  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (policy_data && policy_data->has_request_token()) {
    base::UmaHistogramBoolean(kDMTokenCheckHistogram, true);
    base::UmaHistogramBoolean(kPolicyCheckHistogram, true);
    return;
  }

  base::UmaHistogramBoolean(kDMTokenCheckHistogram, false);
  base::UmaHistogramBoolean(kPolicyCheckHistogram, policy_data);

  std::stringstream debug_info;
  debug_info << "has_policy: " << (policy_data != nullptr);
  // Log the value of the data from policy_fetch_response.
  const em::PolicyFetchResponse* policy_fetch_response =
      device_settings_service_->policy_fetch_response();
  debug_info << ", has_fetch_response: " << (policy_fetch_response != nullptr);
  if (policy_fetch_response) {
    debug_info << ", has_signature: "
               << policy_fetch_response->has_policy_data_signature();
    debug_info << ", size = " << policy_fetch_response->ByteSize();
    std::unique_ptr<em::PolicyData> poldata =
        std::make_unique<em::PolicyData>();
    if (!policy_fetch_response->has_policy_data() ||
        !poldata->ParseFromString(policy_fetch_response->policy_data()) ||
        !poldata->IsInitialized()) {
      debug_info << ", parse policy failed";
    } else {
      debug_info << ", has_dm_token: " << poldata->has_request_token();
      if (poldata->has_request_token()) {
        debug_info << ", dm_token size: " << poldata->request_token().size();
      }
      debug_info << ", has_device_id: " << poldata->has_device_id()
                 << ", has_device_state: " << poldata->has_device_state();
    }
  }
  debug_info << ", attrs mode: " << install_attributes_->GetMode()
             << ", is_locked: " << install_attributes_->IsDeviceLocked();
  LOG(ERROR) << "Device policy read on enrolled device yields "
             << "no DM token! Status: " << service_status
             << ", debug_info: " << debug_info.str() << ".";

  // At the time LoginDisplayHostWebUI decides whether enrollment flow is to
  // be started, policy hasn't been read yet.  To work around this, once the
  // need for recovery is detected upon policy load, a flag is stored in prefs
  // which is accessed by LoginDisplayHostWebUI early during (next) boot.
  ash::StartupUtils::MarkEnrollmentRecoveryRequired();
}

void DeviceCloudPolicyStoreAsh::UpdateFirstPoliciesLoaded() {
  CloudPolicyStore::UpdateFirstPoliciesLoaded();
  // Mark policies as loaded if we don't expect any policies to be loaded.
  first_policies_loaded_ |= !install_attributes_->IsEnterpriseManaged();
}

}  // namespace policy
