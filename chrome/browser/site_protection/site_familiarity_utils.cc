// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_utils.h"

#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/common/content_features.h"

namespace site_protection {

bool AreV8OptimizationsDisabledOnUnfamiliarSites(const PrefService& prefs) {
  return base::FeatureList::IsEnabled(
             features::kProcessSelectionDeferringConditions) &&
         base::FeatureList::IsEnabled(
             content_settings::features::
                 kBlockV8OptimizerOnUnfamiliarSitesSetting) &&
         prefs.GetBoolean(
             prefs::kJavascriptOptimizerBlockedForUnfamiliarSites) &&
         safe_browsing::IsSafeBrowsingEnabled(prefs);
}

}  // namespace site_protection
