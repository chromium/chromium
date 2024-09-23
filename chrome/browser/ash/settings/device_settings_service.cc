// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_policy_applier.h"
#include "chrome/browser/ash/settings/session_manager_operation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#include "crypto/rsa_private_key.h"

namespace em = enterprise_management;

using ownership::OwnerKeyUtil;
using ownership::PublicKey;
using policy::PolicyDeviceIdValidity;

namespace ash {
namespace {

constexpr char kDeviceIdValidityOldEnrollmentEnterprise[] =
    "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentEnterprise";
constexpr char kDeviceIdValidityOldEnrollmentDemo[] =
    "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentDemo";
constexpr char kDeviceIdValidityNewEnrollmentEnterprise[] =
    "Enterprise.DevicePolicyDeviceIdValidity2.NewEnrollmentEnterprise";
constexpr char kDeviceIdValidityNewEnrollmentDemo[] =
    "Enterprise.DevicePolicyDeviceIdValidity2.NewEnrollmentDemo";
constexpr char kDeviceLocalAccountCount[] =
    "Enterprise.DeviceLocalAccountCount";

void RecordDeviceIdValidityMetric(em::PolicyData* policy_data) {
  // The enrollment version pref was added in M121. All the devices enrolled
  // before that don't have the pref on local state.
  auto* local_state = g_browser_process->local_state();
  const bool is_old_enrollment =
      !local_state ||
      local_state->GetString(prefs::kEnrollmentVersionOS).empty();
  InstallAttributes* install_attributes = InstallAttributes::Get();
  const bool is_demo_mode =
      install_attributes->GetMode() == policy::DEVICE_MODE_DEMO;

  PolicyDeviceIdValidity device_id_validity = PolicyDeviceIdValidity::kValid;
  if (install_attributes->GetDeviceId().empty()) {
    device_id_validity = PolicyDeviceIdValidity::kActualIdUnknown;
  } else if (!policy_data->has_device_id()) {
    device_id_validity = PolicyDeviceIdValidity::kMissing;
  } else if (policy_data->device_id() != install_attributes->GetDeviceId()) {
    device_id_validity = PolicyDeviceIdValidity::kInvalid;
  }

  const char* histogram_name = kDeviceIdValidityNewEnrollmentEnterprise;
  if (is_demo_mode) {
    histogram_name = is_old_enrollment ? kDeviceIdValidityOldEnrollmentDemo
                                       : kDeviceIdValidityNewEnrollmentDemo;
  } else if (is_old_enrollment) {
    histogram_name = kDeviceIdValidityOldEnrollmentEnterprise;
  }
  base::UmaHistogramEnumeration(histogram_name, device_id_validity);
}

void RecordDeviceLocalAccountsMetric(
    const em::ChromeDeviceSettingsProto& settings) {
  base::UmaHistogramCustomCounts(
      kDeviceLocalAccountCount,
      settings.device_local_accounts().account().size(),
      /*min=*/1,
      /*exclusive_max=*/500,
      /*buckets=*/100);
}

}  // namespace

DeviceSettingsService::Observer::~Observer() {}

void DeviceSettingsService::Observer::OwnershipStatusChanged() {}

void DeviceSettingsService::Observer::DeviceSettingsUpdated() {}

void DeviceSettingsService::Observer::OnDeviceSettingsServiceShutdown() {}

static DeviceSettingsService* g_device_settings_service = nullptr;

// static
void DeviceSettingsService::Initialize() {
  CHECK(!g_device_settings_service);
  g_device_settings_service = new DeviceSettingsService();
}

// static
bool DeviceSettingsService::IsInitialized() {
  return g_device_settings_service;
}

// static
void DeviceSettingsService::Shutdown() {
  DCHECK(g_device_settings_service);
  delete g_device_settings_service;
  g_device_settings_service = nullptr;
}

// static
DeviceSettingsService* DeviceSettingsService::Get() {
  CHECK(g_device_settings_service);
  return g_device_settings_service;
}

// static
const char* DeviceSettingsService::StatusToString(Status status) {
  switch (status) {
    case STORE_SUCCESS:
      return "SUCCESS";
    case STORE_KEY_UNAVAILABLE:
      return "KEY_UNAVAILABLE";
    case STORE_OPERATION_FAILED:
      return "OPERATION_FAILED";
    case STORE_NO_POLICY:
      return "NO_POLICY";
    case STORE_INVALID_POLICY:
      return "INVALID_POLICY";
    case STORE_VALIDATION_ERROR:
      return "VALIDATION_ERROR";
  }
  return "UNKNOWN";
}

DeviceSettingsService::DeviceSettingsService() {
  device_off_hours_controller_ =
      std::make_unique<policy::off_hours::DeviceOffHoursController>();
}

DeviceSettingsService::~DeviceSettingsService() {
  DCHECK(pending_operations_.empty());
  for (auto& observer : observers_)
    observer.OnDeviceSettingsServiceShutdown();
}

void DeviceSettingsService::SetSessionManager(
    SessionManagerClient* session_manager_client,
    scoped_refptr<OwnerKeyUtil> owner_key_util) {
  DCHECK(session_manager_client);
  DCHECK(owner_key_util.get());
  DCHECK(!session_manager_client_);
  DCHECK(!owner_key_util_.get());

  session_manager_client_ = session_manager_client;
  owner_key_util_ = owner_key_util;

  session_manager_client_->AddObserver(this);

  StartNextOperation();
}

void DeviceSettingsService::UnsetSessionManager() {
  pending_operations_.clear();

  if (session_manager_client_)
    session_manager_client_->RemoveObserver(this);
  session_manager_client_ = nullptr;
  owner_key_util_.reset();
}

void DeviceSettingsService::SetDeviceMode(policy::DeviceMode device_mode) {
  if (device_mode_ == device_mode)
    return;

  // Device mode can only change if was not set yet.
  DCHECK(policy::DEVICE_MODE_PENDING == device_mode_ ||
         policy::DEVICE_MODE_NOT_SET == device_mode_);
  device_mode_ = device_mode;
  if (GetOwnershipStatus() != OwnershipStatus::kOwnershipUnknown) {
    RunPendingOwnershipStatusCallbacks();
  }
}

scoped_refptr<PublicKey> DeviceSettingsService::GetPublicKey() {
  return public_key_;
}

void DeviceSettingsService::SetDeviceOffHoursControllerForTesting(
    std::unique_ptr<policy::off_hours::DeviceOffHoursController> controller) {
  device_off_hours_controller_ = std::move(controller);
}

void DeviceSettingsService::Load() {
  EnqueueLoad(false);
}

void DeviceSettingsService::LoadIfNotPresent() {
  // Return if there is already some policy loaded (policy_fetch_response_), or
  // if there are any pending operations (|pending_operations_| not empty). The
  // pending operations can be of two types only, load or store. If a loading
  // operation is in the queue, then a request to load will be issued soon. If a
  // store operation is pending, then the policy data is already available and
  // will be stored soon. In either case, it's not needed to load again.
  if (policy_fetch_response_ || !pending_operations_.empty())
    return;

  EnqueueLoad(false);
}

void DeviceSettingsService::LoadImmediately() {
  if (session_stopping_) {
    LOG(WARNING) << "Fail the blocking request when the session is stopping";
    // No need to HandleCompletedOperation, as there's no callback waiting.
    return;
  }
  std::unique_ptr<SessionManagerOperation> operation(new LoadSettingsOperation(
      /*request_key_load=*/true, /*force_immediate_load=*/true,
      base::BindOnce(&DeviceSettingsService::HandleCompletedOperation,
                     weak_factory_.GetWeakPtr(), base::OnceClosure())));
  operation->Start(session_manager_client_, owner_key_util_, public_key_);
}

void DeviceSettingsService::Store(
    std::unique_ptr<em::PolicyFetchResponse> policy,
    base::OnceClosure callback) {
  Enqueue(std::make_unique<StoreSettingsOperation>(
      base::BindOnce(&DeviceSettingsService::HandleCompletedAsyncOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      std::move(policy)));
}

DeviceSettingsService::OwnershipStatus
DeviceSettingsService::GetOwnershipStatus() {
  if (public_key_.get())
    return public_key_->is_empty() ? OwnershipStatus::kOwnershipNone
                                   : OwnershipStatus::kOwnershipTaken;
  return OwnershipStatus::kOwnershipUnknown;
}

void DeviceSettingsService::GetOwnershipStatusAsync(
    OwnershipStatusCallback callback) {
  if (GetOwnershipStatus() != OwnershipStatus::kOwnershipUnknown) {
    // Report status immediately.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DeviceSettingsService::ValidateOwnershipStatusAndNotify,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    // If the key hasn't been loaded yet, enqueue the callback to be fired when
    // the next SessionManagerOperation completes. If no operation is pending,
    // start a load operation to fetch the key and report the result.
    pending_ownership_status_callbacks_.push_back(std::move(callback));
    if (pending_operations_.empty())
      EnqueueLoad(false);
  }
}

void DeviceSettingsService::ValidateOwnershipStatusAndNotify(
    OwnershipStatusCallback callback) {
  if (GetOwnershipStatus() == OwnershipStatus::kOwnershipUnknown) {
    // OwnerKeySet() could be called upon user sign-in while event was in queue,
    // which resets status to OwnershipStatus::kOwnershipUnknown.
    // We need to retry the logic in this case.
    GetOwnershipStatusAsync(std::move(callback));
    return;
  }
  std::move(callback).Run(GetOwnershipStatus());
}

bool DeviceSettingsService::HasPrivateOwnerKey() {
  return owner_settings_service_ && owner_settings_service_->IsOwner();
}

void DeviceSettingsService::InitOwner(
    const std::string& username,
    const base::WeakPtr<ownership::OwnerSettingsService>&
        owner_settings_service) {
  // When InitOwner() is called twice with the same |username| it's
  // worth to reload settings since owner key may become available.
  if (!username_.empty() && username_ != username)
    return;
  username_ = username;
  owner_settings_service_ = owner_settings_service;

  // Reset the flag since consumer ownership should be established now.
  will_establish_consumer_ownership_ = false;

  EnsureReload(true);
}

const std::string& DeviceSettingsService::GetUsername() const {
  return username_;
}

ownership::OwnerSettingsService*
DeviceSettingsService::GetOwnerSettingsService() const {
  return owner_settings_service_.get();
}

void DeviceSettingsService::MarkWillEstablishConsumerOwnership() {
  will_establish_consumer_ownership_ = true;
}

void DeviceSettingsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceSettingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceSettingsService::OwnerKeySet(bool success) {
  if (!success) {
    LOG(ERROR) << "Owner key change failed.";
    return;
  }

  public_key_.reset();

  if (!will_establish_consumer_ownership_) {
    EnsureReload(true);
  }
}

void DeviceSettingsService::PropertyChangeComplete(bool success) {
  if (!success) {
    LOG(ERROR) << "Policy update failed.";
    return;
  }

  if (GetOwnershipStatus() == OwnershipStatus::kOwnershipTaken ||
      !will_establish_consumer_ownership_) {
    EnsureReload(false);
  }
}

void DeviceSettingsService::SessionStopping() {
  session_stopping_ = true;
}

void DeviceSettingsService::Enqueue(
    std::unique_ptr<SessionManagerOperation> operation) {
  const bool was_empty = pending_operations_.empty();
  pending_operations_.push_back(std::move(operation));
  if (was_empty)
    StartNextOperation();
}

void DeviceSettingsService::EnqueueLoad(bool request_key_load) {
  Enqueue(std::make_unique<LoadSettingsOperation>(
      request_key_load, /*force_immediate_load=*/false,
      base::BindOnce(&DeviceSettingsService::HandleCompletedAsyncOperation,
                     weak_factory_.GetWeakPtr(), base::OnceClosure())));
}

void DeviceSettingsService::EnsureReload(bool request_key_load) {
  if (!pending_operations_.empty())
    pending_operations_.front()->RestartLoad(request_key_load);
  else
    EnqueueLoad(request_key_load);
}

void DeviceSettingsService::StartNextOperation() {
  if (!pending_operations_.empty() && session_manager_client_ &&
      owner_key_util_.get()) {
    pending_operations_.front()->Start(session_manager_client_, owner_key_util_,
                                       public_key_);
  }
}

void DeviceSettingsService::HandleCompletedAsyncOperation(
    base::OnceClosure callback,
    SessionManagerOperation* operation,
    Status status) {
  DCHECK_EQ(operation, pending_operations_.front().get());
  HandleCompletedOperation(std::move(callback), operation, status);
  // Only remove the pending operation here, so new operations triggered by
  // any of the callbacks above are queued up properly.
  pending_operations_.pop_front();

  StartNextOperation();
}

void DeviceSettingsService::HandleCompletedOperation(
    base::OnceClosure callback,
    SessionManagerOperation* operation,
    Status status) {
  store_status_ = status;
  if (status == STORE_SUCCESS) {
    policy_fetch_response_ = std::move(operation->policy_fetch_response());
    policy_data_ = std::move(operation->policy_data());
    device_settings_ = std::move(operation->device_settings());
    // Log histograms only if the device is managed and the policy is in
    // good state.
    if (policy_data_ && policy_data_->has_request_token() &&
        InstallAttributes::Get()->IsEnterpriseManaged()) {
      RecordDeviceIdValidityMetric(policy_data_.get());
      RecordDeviceLocalAccountsMetric(*device_settings_);
    }
    // Update "OffHours" policy state and apply "OffHours" policy to current
    // proto only during "OffHours" mode. When "OffHours" mode begins and ends
    // DeviceOffHoursController requests DeviceSettingsService to asynchronously
    // reload device policies. (See |DeviceOffHoursController| class
    // description)
    device_off_hours_controller_->UpdateOffHoursPolicy(*device_settings_);
    if (device_off_hours_controller_->is_off_hours_mode()) {
      std::unique_ptr<em::ChromeDeviceSettingsProto> off_device_settings =
          policy::off_hours::ApplyOffHoursPolicyToProto(*device_settings_);
      if (off_device_settings)
        device_settings_.swap(off_device_settings);
    }
  } else if (status != STORE_KEY_UNAVAILABLE) {
    LOG(ERROR) << "Session manager operation failed: " << status << " ("
               << StatusToString(status) << ")";
  }

  public_key_ = scoped_refptr<PublicKey>(operation->public_key());
  if (GetOwnershipStatus() != previous_ownership_status_) {
    // TODO(b/293598969): Investigate onwerhip status condition to prevent the
    // bugs in future. Check whether ownership status goes to kOwnershipTaken in
    // the end. Remove this when it's resolved.
    LOG(WARNING) << "Ownership status is changed from "
                 << previous_ownership_status_ << " to "
                 << GetOwnershipStatus();

    previous_ownership_status_ = GetOwnershipStatus();
    NotifyOwnershipStatusChanged();
  }
  NotifyDeviceSettingsUpdated();
  RunPendingOwnershipStatusCallbacks();

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    std::move(callback).Run();
}

void DeviceSettingsService::NotifyOwnershipStatusChanged() const {
  for (auto& observer : observers_)
    observer.OwnershipStatusChanged();
}

void DeviceSettingsService::NotifyDeviceSettingsUpdated() const {
  for (auto& observer : observers_)
    observer.DeviceSettingsUpdated();
}

void DeviceSettingsService::RunPendingOwnershipStatusCallbacks() {
  std::vector<OwnershipStatusCallback> callbacks;
  callbacks.swap(pending_ownership_status_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(GetOwnershipStatus());
  }
}

bool DeviceSettingsService::IsDeviceManaged() const {
  if (!policy_data_ || policy_data_->state() != em::PolicyData::ACTIVE) {
    return false;
  }
  if (policy_data_->has_management_mode()) {
    return policy_data_->management_mode() ==
           em::PolicyData::ENTERPRISE_MANAGED;
  } else {
    // The old device settings didn't have a management_mode. For those we
    // have to rely on the presence of request_token.
    return policy_data_->has_request_token();
  }
}

bool DeviceSettingsService::HasDmToken() const {
  return policy_data_ && policy_data_->has_request_token();
}

std::ostream& operator<<(std::ostream& ostream,
                         DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case DeviceSettingsService::OwnershipStatus::kOwnershipUnknown:
      return ostream << "kOwnershipUnknown";
    case DeviceSettingsService::OwnershipStatus::kOwnershipNone:
      return ostream << "kOwnershipNone";
    case DeviceSettingsService::OwnershipStatus::kOwnershipTaken:
      return ostream << "kOwnershipTaken";
  }
}

ScopedTestDeviceSettingsService::ScopedTestDeviceSettingsService() {
  DeviceSettingsService::Initialize();
}

ScopedTestDeviceSettingsService::~ScopedTestDeviceSettingsService() {
  // Clean pending operations.
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

}  // namespace ash
