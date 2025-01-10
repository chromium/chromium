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
bool GlicEnabling::IsEnabledForProfile(const Profile* profile) {
  CHECK(profile);
  if (!IsEnabledByFlags()) {
    return false;
  }

  // Glic is supported only in regular profiles, i.e. disable in incognito,
  // guest, system profile, etc.
  if (!profile->IsRegularProfile()) {
    return false;
  }

  return profile->GetPrefs()->GetBoolean(glic::prefs::kGlicEnabledByPolicy);
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
