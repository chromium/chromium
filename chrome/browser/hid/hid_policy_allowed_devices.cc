// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_policy_allowed_devices.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

// static
void HidPolicyAllowedDevices::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedWebHidAllowAllDevicesForUrls);
  registry->RegisterListPref(prefs::kManagedWebHidAllowDevicesForUrls);
  registry->RegisterListPref(
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls);
}
