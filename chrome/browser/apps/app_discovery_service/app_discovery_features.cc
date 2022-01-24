// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace apps {

const base::Feature kAppDiscoveryRemoteUrlSearch{
    "AppDiscoveryRemoteUrlSearch", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsRemoteUrlSearchEnabled() {
  return base::FeatureList::IsEnabled(kAppDiscoveryRemoteUrlSearch);
}

}  // namespace apps
