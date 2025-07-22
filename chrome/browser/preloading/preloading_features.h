// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_
#define CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// DSE Prewarm tracked at https://crbug.com/406378765.
BASE_DECLARE_FEATURE(kPrewarm);
BASE_DECLARE_FEATURE_PARAM(std::string, kPrewarmUrl);
BASE_DECLARE_FEATURE_PARAM(bool, kPrewarmZeroSuggestTrigger);
BASE_DECLARE_FEATURE_PARAM(bool, kForceEnableWithDevTools);

// If enabled, requests the compositor warm-up (crbug.com/41496019) for
// each prerender trigger.
BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForBookmarkBar);
BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForNewTabPage);

}  // namespace features

#endif  // CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_
