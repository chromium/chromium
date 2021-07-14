// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include "ash/constants/ash_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace {

const char kDefaultDeviceName[] = "ChromeOS";

// This will point to the singleton instance upon initialization.
DeviceNameStore* g_instance = nullptr;

}  // namespace

DeviceNameStore::~DeviceNameStore() = default;

// static
DeviceNameStore* DeviceNameStore::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(g_instance);
  return g_instance;
}

// static
void DeviceNameStore::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterStringPref(prefs::kDeviceName, "");
}

// static
void DeviceNameStore::Initialize(PrefService* prefs) {
  CHECK(base::FeatureList::IsEnabled(features::kEnableHostnameSetting));
  CHECK(!g_instance);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_instance = new DeviceNameStore(prefs);
}

// static
void DeviceNameStore::Shutdown() {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }
}

DeviceNameStore::DeviceNameStore(PrefService* prefs) : prefs_(prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  if (prefs_->GetString(prefs::kDeviceName).empty()) {
    prefs_->SetString(prefs::kDeviceName, kDefaultDeviceName);
  }
}

std::string DeviceNameStore::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

}  // namespace chromeos
