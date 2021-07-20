// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store_impl.h"

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
    : prefs_(prefs), handler_(handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  if (prefs_->GetString(prefs::kDeviceName).empty()) {
    prefs_->SetString(prefs::kDeviceName, kDefaultDeviceName);
  }
  handler_->AddObserver(this);
}

DeviceNameStoreImpl::~DeviceNameStoreImpl() {
  handler_->RemoveObserver(this);
}

std::string DeviceNameStoreImpl::GetDeviceName() const {
  policy::DeviceNamePolicyHandler::DeviceNamePolicy device_name_policy =
      handler_->GetDeviceNamePolicy();
  if (device_name_policy == policy::DeviceNamePolicyHandler::DeviceNamePolicy::
                                kPolicyHostnameChosenByAdmin) {
    return *handler_->GetHostnameChosenByAdministrator();
  } else if (device_name_policy ==
             policy::DeviceNamePolicyHandler::DeviceNamePolicy::
                 kPolicyHostnameNotConfigurable) {
    return kDefaultDeviceName;
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(prefs_);
    return prefs_->GetString(prefs::kDeviceName);
  }
}

void DeviceNameStoreImpl::OnHostnamePolicyChanged() {
  // TODO: Update name in set in |prefs|
}

}  // namespace chromeos
