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
}

DeviceNameStoreImpl::DeviceNameStoreImpl(PrefService* prefs) : prefs_(prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  if (prefs_->GetString(prefs::kDeviceName).empty()) {
    prefs_->SetString(prefs::kDeviceName, kDefaultDeviceName);
  }
}

DeviceNameStoreImpl::~DeviceNameStoreImpl() = default;

std::string DeviceNameStoreImpl::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

}  // namespace chromeos
