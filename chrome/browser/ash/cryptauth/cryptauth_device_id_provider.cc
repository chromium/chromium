// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cryptauth/cryptauth_device_id_provider.h"

#include "base/uuid.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::cryptauth_device_id {

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthDeviceId, std::string());
}

std::string GetDeviceID(PrefService& local_state) {
  std::string device_id = local_state.GetString(prefs::kCryptAuthDeviceId);
  if (device_id.empty()) {
    device_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    local_state.SetString(prefs::kCryptAuthDeviceId, device_id);
  }
  return device_id;
}

}  // namespace ash::cryptauth_device_id
