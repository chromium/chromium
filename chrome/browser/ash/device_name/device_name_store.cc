// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_store.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/device_name/device_name_store_impl.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace {

// This will point to the singleton instance upon initialization.
DeviceNameStore* g_instance = nullptr;

}  // namespace

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
void DeviceNameStore::Initialize(PrefService* prefs,
                                 policy::DeviceNamePolicyHandler* handler) {
  CHECK(base::FeatureList::IsEnabled(features::kEnableHostnameSetting));
  CHECK(!g_instance);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_instance = new DeviceNameStoreImpl(prefs, handler);
}

// static
void DeviceNameStore::Shutdown() {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }
}

DeviceNameStore::DeviceNameStore() = default;

DeviceNameStore::~DeviceNameStore() = default;

void DeviceNameStore::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceNameStore::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DeviceNameStore::NotifyDeviceNameMetadataChanged() {
  for (auto& observer : observer_list_) {
    observer.OnDeviceNameMetadataChanged();
  }
}

}  // namespace ash
