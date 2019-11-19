// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_

#include "base/feature_list.h"

namespace prerender {

// These Finch feature and parameter strings exposed for for testing.
extern const base::Feature kNoStatePrefetchFeature;

// Preconnects instead of prefetching from GWS.
extern const base::Feature kGWSPrefetchHoldback;

// Preconnects instead of prefetching from NavigationPredictor.
extern const base::Feature kNavigationPredictorPrefetchHoldback;

// Configures global state using kNoStatePrefetchFeature.
void ConfigureNoStatePrefetch();

bool IsNoStatePrefetchEnabled();

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_FIELD_TRIAL_H_
