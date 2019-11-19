// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_policy_applier.h"
#include "chrome/browser/chromeos/settings/session_manager_operation.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

#include "crypto/rsa_private_key.h"

namespace em = enterprise_management;

using ownership::OwnerKeyUtil;
using ownership::PublicKey;

namespace chromeos {

DeviceSettingsService::Observer::~Observer() {}

void DeviceSettingsService::Observer::OwnershipStatusChanged() {}

void DeviceSettingsService::Observer::DeviceSettingsUpdated() {}

void DeviceSettingsService::Observer::OnDeviceSettingsServiceShutdown() {}

static DeviceSettingsService* g_device_settings_service = NULL;

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
  g_device_settings_service = NULL;
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
  session_manager_client_ = NULL;
  owner_key_util_.reset();
}

void DeviceSettingsService::SetDeviceMode(policy::DeviceMode device_mode) {
  if (device_mode_ == device_mode)
    return;

  // Device mode can only change if was not set yet.
  DCHECK(policy::DEVICE_MODE_PENDING == device_mode_ ||
         policy::DEVICE_MODE_NOT_SET == device_mode_);
  device_mode_ = device_mode;
  if (GetOwnershipStatus() != OWNERSHIP_UNKNOWN) {
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

void DeviceSettingsService::LoadImmediately() {
  bool request_key_load = true;
  bool cloud_validations = true;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    request_key_load = false;
    cloud_validations = false;
  }
  std::unique_ptr<SessionManagerOperation> operation(new LoadSettingsOperation(
      request_key_load, cloud_validations, true /*force_immediate_load*/,
      base::Bind(&DeviceSettingsService::HandleCompletedOperation,
                 weak_factory_.GetWeakPtr(), base::Closure())));
  operation->Start(session_manager_client_, owner_key_util_, public_key_);
}

void DeviceSettingsService::Store(
    std::unique_ptr<em::PolicyFetchResponse> policy,
    const base::Closure& callback) {
  // On Active Directory managed devices policy is written only by authpolicyd.
  CHECK(device_mode_ != policy::DEVICE_MODE_ENTERPRISE_AD);
  Enqueue(std::make_unique<StoreSettingsOperation>(
      base::Bind(&DeviceSettingsService::HandleCompletedAsyncOperation,
                 weak_factory_.GetWeakPtr(), callback),
      std::move(policy)));
}

DeviceSettingsService::OwnershipStatus
    DeviceSettingsService::GetOwnershipStatus() {
  if (public_key_.get())
    return public_key_->is_loaded() ? OWNERSHIP_TAKEN : OWNERSHIP_NONE;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD)
    return OWNERSHIP_TAKEN;
  return OWNERSHIP_UNKNOWN;
}

void DeviceSettingsService::GetOwnershipStatusAsync(
    const OwnershipStatusCallback& callback) {
  if (GetOwnershipStatus() != OWNERSHIP_UNKNOWN) {
    // Report status immediately.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, GetOwnershipStatus()));
  } else {
    // If the key hasn't been loaded yet, enqueue the callback to be fired when
    // the next SessionManagerOperation completes. If no operation is pending,
    // start a load operation to fetch the key and report the result.
    pending_ownership_status_callbacks_.push_back(callback);
    if (pending_operations_.empty())
      EnqueueLoad(false);
  }
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

  if (GetOwnershipStatus() == OWNERSHIP_TAKEN ||
      !will_establish_consumer_ownership_) {
    EnsureReload(true);
  }
}

void DeviceSettingsService::PropertyChangeComplete(bool success) {
  if (!success) {
    LOG(ERROR) << "Policy update failed.";
    return;
  }

  if (GetOwnershipStatus() == OWNERSHIP_TAKEN ||
      !will_establish_consumer_ownership_) {
    EnsureReload(false);
  }
}

void DeviceSettingsService::Enqueue(
    std::unique_ptr<SessionManagerOperation> operation) {
  const bool was_empty = pending_operations_.empty();
  pending_operations_.push_back(std::move(operation));
  if (was_empty)
    StartNextOperation();
}

void DeviceSettingsService::EnqueueLoad(bool request_key_load) {
  bool cloud_validations = true;
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    request_key_load = false;
    cloud_validations = false;
  }
  Enqueue(std::make_unique<LoadSettingsOperation>(
      request_key_load, cloud_validations, false /*force_immediate_load*/,
      base::Bind(&DeviceSettingsService::HandleCompletedAsyncOperation,
                 weak_factory_.GetWeakPtr(), base::Closure())));
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
    pending_operations_.front()->Start(
        session_manager_client_, owner_key_util_, public_key_);
  }
}

void DeviceSettingsService::HandleCompletedAsyncOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    Status status) {
  DCHECK_EQ(operation, pending_operations_.front().get());
  HandleCompletedOperation(callback, operation, status);
  // Only remove the pending operation here, so new operations triggered by
  // any of the callbacks above are queued up properly.
  pending_operations_.pop_front();

  StartNextOperation();
}

void DeviceSettingsService::HandleCompletedOperation(
    const base::Closure& callback,
    SessionManagerOperation* operation,
    Status status) {
  store_status_ = status;
  if (status == STORE_SUCCESS) {
    policy_data_ = std::move(operation->policy_data());
    device_settings_ = std::move(operation->device_settings());
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
    previous_ownership_status_ = GetOwnershipStatus();
    NotifyOwnershipStatusChanged();
  }
  NotifyDeviceSettingsUpdated();
  RunPendingOwnershipStatusCallbacks();

  // The completion callback happens after the notification so clients can
  // filter self-triggered updates.
  if (!callback.is_null())
    callback.Run();
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
  for (const auto& callback : callbacks) {
    callback.Run(GetOwnershipStatus());
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

}  // namespace chromeos
