// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"

bool GlicEnabling::IsEnabledByFlags() {
  return CheckEnabling() == glic::GlicEnabledStatus::kEnabled;
}

// static
bool GlicEnabling::IsProfileEligible(const Profile* profile) {
  CHECK(profile);
  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  return IsEnabledByFlags() && profile->IsRegularProfile();
}

// static
bool GlicEnabling::IsEnabledForProfile(const Profile* profile) {
  if (!IsProfileEligible(profile)) {
    return false;
  }

  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicSettingsPolicy) ==
         static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled);
}

glic::GlicEnabledStatus GlicEnabling::CheckEnabling() {
  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(features::kGlic)) {
    return glic::GlicEnabledStatus::kGlicFeatureFlagDisabled;
  }
  if (!base::FeatureList::IsEnabled(features::kTabstripComboButton)) {
    return glic::GlicEnabledStatus::kTabstripComboButtonDisabled;
  }
  return glic::GlicEnabledStatus::kEnabled;
}
