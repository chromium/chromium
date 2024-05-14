// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"

namespace extensions {
namespace settings_private {

void SetPrefFromSource(sync_preferences::TestingPrefServiceSyncable* prefs,
                       const std::string& pref_name,
                       settings_private::PrefSetting pref_setting,
                       settings_private::PrefSource source) {
  if (pref_setting == settings_private::PrefSetting::kNotSet) {
    return;
  }
  auto pref_value = std::make_unique<base::Value>(
      pref_setting == settings_private::PrefSetting::kRecommendedOn ||
      pref_setting == settings_private::PrefSetting::kEnforcedOn);
  if (source == settings_private::PrefSource::kExtension) {
    prefs->SetExtensionPref(pref_name, std::move(pref_value));
  } else if (source == settings_private::PrefSource::kDevicePolicy) {
    prefs->SetManagedPref(pref_name, std::move(pref_value));
  } else if (source == settings_private::PrefSource::kRecommended) {
    prefs->SetRecommendedPref(pref_name, std::move(pref_value));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void TestGeneratedPrefObserver::OnGeneratedPrefChanged(
    const std::string& pref_name) {
  updated_pref_name_ = pref_name;
}

}  // namespace settings_private
}  // namespace extensions
