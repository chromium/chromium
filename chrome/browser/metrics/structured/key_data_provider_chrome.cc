// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/key_data_provider_chrome.h"

#include <optional>

#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics::structured {

KeyDataProviderChrome::KeyDataProviderChrome(PrefService* local_state)
    : device_key_(local_state, prefs::kDeviceKeyDataPrefName) {}

KeyDataProviderChrome::~KeyDataProviderChrome() = default;

bool KeyDataProviderChrome::IsReady() {
  return device_key_.IsReady();
}

std::optional<uint64_t> KeyDataProviderChrome::GetId(
    const std::string& project_name) {
  return device_key_.GetId(project_name);
}

KeyData* KeyDataProviderChrome::GetKeyData(const std::string& project_name) {
  return device_key_.GetKeyData(project_name);
}

void KeyDataProviderChrome::Purge() {
  device_key_.Purge();
}

// static:
void KeyDataProviderChrome::RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceKeyDataPrefName);
}

}  // namespace metrics::structured
