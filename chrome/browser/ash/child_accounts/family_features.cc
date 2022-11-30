// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_features.h"

namespace ash {

BASE_FEATURE(kFamilyLinkOobeHandoff,
             "FamilyLinkOobeHandoff",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsFamilyLinkOobeHandoffEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOobeHandoff);
}

}  // namespace ash
