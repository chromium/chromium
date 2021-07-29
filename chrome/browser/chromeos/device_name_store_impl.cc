// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store_impl.h"

#include "chrome/browser/chromeos/device_name_applier_impl.h"
#include "chrome/browser/chromeos/device_name_validator.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

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
  observation_.Observe(handler_);

  // Gets the device name according to the device name policy set. If empty, the
  // name in prefs is not set yet and hence we set it to the default name.
  std::string device_name = ComputeDeviceName();

  if (device_name.empty())
    device_name = kDefaultDeviceName;

  ChangeDeviceName(device_name);
}

DeviceNameStoreImpl::~DeviceNameStoreImpl() = default;

std::string DeviceNameStoreImpl::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

bool DeviceNameStoreImpl::IsConfiguringDeviceNameProhibitedByPolicy() {
  switch (handler_->GetDeviceNamePolicy()) {
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable:
      FALLTHROUGH;
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      return false;

    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameConfigurableByManagedUser:
      FALLTHROUGH;
    case policy::DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy:
      return true;
  }
}

void DeviceNameStoreImpl::ChangeDeviceName(const std::string& device_name) {
  device_name_applier_->SetDeviceName(device_name);
  prefs_->SetString(prefs::kDeviceName, device_name);
  NotifyDeviceNameChanged();
}

void DeviceNameStoreImpl::AttemptDeviceNameChange(
    const std::string& device_name) {
  if (GetDeviceName() == device_name)
    return;

  ChangeDeviceName(device_name);
}

DeviceNameStore::SetDeviceNameResult DeviceNameStoreImpl::SetDeviceName(
    const std::string& new_device_name) {
  if (!IsConfiguringDeviceNameProhibitedByPolicy())
    return SetDeviceNameResult::kProhibitedByPolicy;

  if (!user_manager::UserManager::Get()->IsCurrentUserOwner())
    return SetDeviceNameResult::kNotDeviceOwner;

  if (!IsValidDeviceName(new_device_name))
    return SetDeviceNameResult::kInvalidName;

  AttemptDeviceNameChange(new_device_name);
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

void DeviceNameStoreImpl::OnHostnamePolicyChanged() {
  AttemptDeviceNameChange(ComputeDeviceName());
}

}  // namespace chromeos
