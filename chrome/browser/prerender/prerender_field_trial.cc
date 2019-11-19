// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"

namespace prerender {

const base::Feature kNoStatePrefetchFeature{"NoStatePrefetch",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kGWSPrefetchHoldback{"GWSPrefetchHoldback",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNavigationPredictorPrefetchHoldback{
    "NavigationPredictorPrefetchHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

void ConfigureNoStatePrefetch() {
  auto mode = PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH;
  if (!base::FeatureList::IsEnabled(kNoStatePrefetchFeature))
    mode = PrerenderManager::PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT;
  PrerenderManager::SetMode(mode);
}

bool IsNoStatePrefetchEnabled() {
  return PrerenderManager::GetMode() ==
         PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH;
}

}  // namespace prerender
