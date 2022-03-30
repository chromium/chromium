// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_utils.h"

#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace prerender_utils {

const base::Feature kHidePrefetchParameter{"HidePrefetchParameter",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const char kDefaultSearchEngineMetricSuffix[] = "DefaultSearchEngine";
const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";

bool IsDirectUrlInputPrerenderEnabled() {
  return blink::features::IsPrerender2Enabled() &&
         base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2);
}

bool IsSearchSuggestionPrerenderEnabled() {
  return blink::features::IsPrerender2Enabled() &&
         base::FeatureList::IsEnabled(
             features::kSupportSearchSuggestionForPrerender2);
}

bool ShouldUpdateCacheEntryManually() {
  return base::FeatureList::IsEnabled(kHidePrefetchParameter);
}

}  // namespace prerender_utils
