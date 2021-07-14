// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include <math.h>

#include "base/rand_util.h"
#include "base/strings/char_traits.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {
const char kDefaultDeviceName[] = "ChromeOS";
}  // namespace

// static
DeviceNameStore* DeviceNameStore::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Singleton<DeviceNameStore>::get();
}

// static
void DeviceNameStore::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterStringPref(prefs::kDeviceName, "");
}

void DeviceNameStore::Initialize(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs);
  prefs_ = prefs;

  const std::string device_name = prefs_->GetString(prefs::kDeviceName);
  if (device_name.empty()) {
    prefs_->SetString(prefs::kDeviceName, kDefaultDeviceName);
  }
}

std::string DeviceNameStore::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

}  // namespace chromeos
