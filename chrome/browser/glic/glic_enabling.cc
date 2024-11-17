// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "chrome/common/chrome_features.h"

bool GlicEnabling::IsEnabled() {
  return CheckEnabling() == glic::GlicEnabledStatus::kEnabled;
}

glic::GlicEnabledStatus GlicEnabling::CheckEnabling() {
  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(features::kGlic)) {
    return glic::GlicEnabledStatus::kGlicFeatureFlagDisabled;
  }
  return glic::GlicEnabledStatus::kEnabled;
}
