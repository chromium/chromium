// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/prefs.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace safety_hub_prefs {

void RegisterSafetyHubAndroidProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kBreachedCredentialsCount, 0);
}

}  // namespace safety_hub_prefs
