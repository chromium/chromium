// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_features.h"

namespace chromeos {

const base::Feature kFamilyUserMetricsProvider{
    "FamilyUserMetricsProvider", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFamilyLinkUserMetricsProvider{
    "FamilyLinkUserMetricsProvider", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFamilyLinkOobeHandoff{"FamilyLinkOobeHandoff",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

bool IsFamilyLinkOobeHandoffEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOobeHandoff);
}

}  // namespace chromeos
