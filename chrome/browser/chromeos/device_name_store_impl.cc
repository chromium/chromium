// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store_impl.h"

#include "chrome/browser/chromeos/device_name_applier_impl.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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
  if (GetDeviceName().empty()) {
    device_name_applier_->SetDeviceName(kDefaultDeviceName);
    prefs_->SetString(prefs::kDeviceName, kDefaultDeviceName);
  }

  handler_->AddObserver(this);
}

DeviceNameStoreImpl::~DeviceNameStoreImpl() {
  handler_->RemoveObserver(this);
}

std::string DeviceNameStoreImpl::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
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

void DeviceNameStoreImpl::UpdateDeviceName() {
  const std::string new_device_name = ComputeDeviceName();
  if (GetDeviceName() == new_device_name)
    return;
  device_name_applier_->SetDeviceName(new_device_name);
  prefs_->SetString(prefs::kDeviceName, new_device_name);
}

void DeviceNameStoreImpl::OnHostnamePolicyChanged() {
  UpdateDeviceName();
}

}  // namespace chromeos
