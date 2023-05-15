// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_store_impl.h"

#include "base/logging.h"
#include "chrome/browser/ash/device_name/device_name_applier_impl.h"
#include "chrome/browser/ash/device_name/device_name_validator.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {
const char kDefaultDeviceName[] = "ChromeOS";
}  // namespace

DeviceNameStoreImpl::DeviceNameStoreImpl(
    PrefService* prefs,
    policy::DeviceNamePolicyHandler* handler)
    : DeviceNameStoreImpl(prefs,
                          handler,
                          std::make_unique<DeviceNameApplierImpl>()) {}

DeviceNameStoreImpl::DeviceNameStoreImpl(
    PrefService* prefs,
    policy::DeviceNamePolicyHandler* handler,
    std::unique_ptr<DeviceNameApplier> device_name_applier)
    : prefs_(prefs),
      handler_(handler),
      device_name_applier_(std::move(device_name_applier)) {
  policy_handler_observation_.Observe(handler_.get());

  if (g_browser_process->profile_manager())
    profile_manager_observer_.Observe(g_browser_process->profile_manager());

  // Gets the device name according to the device name policy set. If empty, the
  // name in prefs is not set yet and hence we set it to the default name.
  std::string device_name = ComputeDeviceName();

  if (device_name.empty())
    device_name = kDefaultDeviceName;

  device_name_state_ = ComputeDeviceNameState();
  ChangeDeviceName(device_name);
}

DeviceNameStoreImpl::~DeviceNameStoreImpl() = default;

std::string DeviceNameStoreImpl::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

DeviceNameStore::DeviceNameMetadata DeviceNameStoreImpl::GetDeviceNameMetadata()
    const {
  return {GetDeviceName(), device_name_state_};
}

DeviceNameStore::DeviceNameState DeviceNameStoreImpl::ComputeDeviceNameState()
    const {
  if (IsConfiguringDeviceNameProhibitedByPolicy())
    return DeviceNameState::kCannotBeModifiedBecauseOfPolicy;

  if (CannotModifyBecauseNotDeviceOwner())
    return DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner;

  return DeviceNameState::kCanBeModified;
}

bool DeviceNameStoreImpl::CannotModifyBecauseNotDeviceOwner() const {
  // If kNoPolicy is not in place, then the device is managed and user does not
  // have to be device owner to be able to change the name.
  if (handler_->GetDeviceNamePolicy() !=
      policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy) {
    return false;
  }

  // If device is unmanaged, then user has to be the device owner to be able to
  // change the name.
  if (is_user_owner_)
    return false;

  return true;
}

bool DeviceNameStoreImpl::IsConfiguringDeviceNameProhibitedByPolicy() const {
  switch (handler_->GetDeviceNamePolicy()) {
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable:
      [[fallthrough]];
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      return true;

    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameConfigurableByManagedUser:
      [[fallthrough]];
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy:
      return false;
  }
}

void DeviceNameStoreImpl::ChangeDeviceName(const std::string& device_name) {
  device_name_applier_->SetDeviceName(device_name);
  prefs_->SetString(prefs::kDeviceName, device_name);
}

void DeviceNameStoreImpl::AttemptDeviceNameUpdate(
    const std::string& new_device_name) {
  std::string old_device_name = GetDeviceName();
  DeviceNameStore::DeviceNameState new_state = ComputeDeviceNameState();

  if (old_device_name == new_device_name && device_name_state_ == new_state)
    return;

  if (old_device_name != new_device_name)
    ChangeDeviceName(new_device_name);

  device_name_state_ = new_state;
  NotifyDeviceNameMetadataChanged();
}

DeviceNameStore::SetDeviceNameResult DeviceNameStoreImpl::SetDeviceName(
    const std::string& new_device_name) {
  if (device_name_state_ == DeviceNameState::kCannotBeModifiedBecauseOfPolicy)
    return SetDeviceNameResult::kProhibitedByPolicy;

  if (device_name_state_ ==
      DeviceNameState::kCannotBeModifiedBecauseNotDeviceOwner)
    return SetDeviceNameResult::kNotDeviceOwner;

  if (!IsValidDeviceName(new_device_name))
    return SetDeviceNameResult::kInvalidName;

  AttemptDeviceNameUpdate(new_device_name);
  return SetDeviceNameResult::kSuccess;
}

std::string DeviceNameStoreImpl::ComputeDeviceName() const {
  switch (handler_->GetDeviceNamePolicy()) {
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      return *handler_->GetHostnameChosenByAdministrator();

    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable:
      return kDefaultDeviceName;

    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameConfigurableByManagedUser:
      return GetDeviceName();

    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy:
      return GetDeviceName();
  }
}

void DeviceNameStoreImpl::AttemptUpdateToComputedDeviceName() {
  AttemptDeviceNameUpdate(ComputeDeviceName());
}

void DeviceNameStoreImpl::OnHostnamePolicyChanged() {
  AttemptUpdateToComputedDeviceName();
}

void DeviceNameStoreImpl::AttemptDeviceNameStateUpdate(bool is_user_owner) {
  is_user_owner_ = is_user_owner;
  AttemptUpdateToComputedDeviceName();
}

void DeviceNameStoreImpl::OnProfileAdded(Profile* profile) {
  DCHECK(profile);

  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
  if (service) {
    service->IsOwnerAsync(
        base::BindOnce(&DeviceNameStoreImpl::AttemptDeviceNameStateUpdate,
                       weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "Owner settings service unavailable for added profile, will "
               "not update device name state.";
  }
}

void DeviceNameStoreImpl::OnProfileManagerDestroying() {
  profile_manager_observer_.Reset();
}

}  // namespace ash
