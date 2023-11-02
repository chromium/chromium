// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_CACHE_H_
#define CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_CACHE_H_

#include <string>

namespace enterprise_management {
class PolicyData;
}

class PrefService;
class PrefRegistrySimple;

namespace ash {

// There is need (metrics at OOBE stage) to store settings (that normally would
// go into DeviceSettings storage) before owner has been assigned (hence no key
// is available). This set of functions serves as a transient storage in that
// case.
namespace device_settings_cache {
// Registers required pref section.
void RegisterPrefs(PrefRegistrySimple* registry);

// Stores a new policy blob inside the cache stored in |local_state|.
bool Store(const enterprise_management::PolicyData& policy,
           PrefService* local_state);

// Retrieves the policy blob from the cache stored in |local_state|.
bool Retrieve(enterprise_management::PolicyData* policy,
              PrefService* local_state);

// Call this after owner has been assigned to persist settings into
// DeviceSettings storage.
void Finalize(PrefService* local_state);

// Used to convert |policy| into a string that is saved to prefs.
std::string PolicyDataToString(const enterprise_management::PolicyData& policy);

}  // namespace device_settings_cache

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_DEVICE_SETTINGS_CACHE_H_
