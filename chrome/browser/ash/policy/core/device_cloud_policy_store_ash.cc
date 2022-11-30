// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/device_policy_decoder.h"
#include "chrome/browser/ash/policy/dev_mode/dev_mode_policy_util.h"
#include "chrome/browser/ash/policy/value_validation/onc_device_policy_value_validator.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kDMTokenCheckHistogram[] = "Enterprise.EnrolledPolicyHasDMToken";
const char kPolicyCheckHistogram[] = "Enterprise.EnrolledDevicePolicyPresent";

void RecordDeviceIdValidityMetric(
    const std::string& histogram_name,
    const em::PolicyData& policy_data,
    const ash::InstallAttributes& install_attributes) {
  PolicyDeviceIdValidity device_id_validity = PolicyDeviceIdValidity::kMaxValue;
  if (install_attributes.GetDeviceId().empty())
    device_id_validity = PolicyDeviceIdValidity::kActualIdUnknown;
  else if (policy_data.device_id().empty())
    device_id_validity = PolicyDeviceIdValidity::kMissing;
  else if (policy_data.device_id() != install_attributes.GetDeviceId())
    device_id_validity = PolicyDeviceIdValidity::kInvalid;
  else
    device_id_validity = PolicyDeviceIdValidity::kValid;
  base::UmaHistogramEnumeration(histogram_name, device_id_validity);
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
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&DeviceCloudPolicyStoreAsh::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr()));
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
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&DeviceCloudPolicyStoreAsh::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr()));
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

      RecordDeviceIdValidityMetric(
          "Enterprise.CachedDevicePolicyDeviceIdValidity", *policy_data,
          *install_attributes_);
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
  NOTREACHED();
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

  // PolicyData from Active Directory doesn't contain a DM token.
  if (install_attributes_->IsActiveDirectoryManaged()) {
    return;
  }

  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (policy_data && policy_data->has_request_token()) {
    base::UmaHistogramBoolean(kDMTokenCheckHistogram, true);
    base::UmaHistogramBoolean(kPolicyCheckHistogram, true);
    return;
  }

  base::UmaHistogramBoolean(kDMTokenCheckHistogram, false);
  base::UmaHistogramBoolean(kPolicyCheckHistogram, policy_data);

  LOG(ERROR) << "Device policy read on enrolled device yields "
             << "no DM token! Status: " << service_status << ".";

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
