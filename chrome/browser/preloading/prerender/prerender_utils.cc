// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_utils.h"

#include "chrome/browser/browser_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace prerender_utils {

BASE_FEATURE(kHidePrefetchParameter,
             "HidePrefetchParameter",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kDefaultSearchEngineMetricSuffix[] = "DefaultSearchEngine";
const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";
const char kBookmarkBarMetricSuffix[] = "BookmarkBar";

bool IsDirectUrlInputPrerenderEnabled() {
  return base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2);
}

bool IsSearchSuggestionPrerenderEnabled() {
  return base::FeatureList::IsEnabled(
      features::kSupportSearchSuggestionForPrerender2);
}

bool ShouldUpdateCacheEntryManually() {
  return base::FeatureList::IsEnabled(kHidePrefetchParameter);
}

bool SearchPrefetchUpgradeToPrerenderIsEnabled() {
  CHECK(IsSearchSuggestionPrerenderEnabled());
  switch (features::kSearchSuggestionPrerenderImplementationTypeParam.Get()) {
    case features::SearchSuggestionPrerenderImplementationType::kUsePrefetch:
      return true;
    case features::SearchSuggestionPrerenderImplementationType::kIgnorePrefetch:
      return false;
  }
}

bool SearchPreloadShareableCacheIsEnabled() {
  if (!SearchPrefetchUpgradeToPrerenderIsEnabled()) {
    return false;
  }
  switch (features::kSearchPreloadShareableCacheTypeParam.Get()) {
    case features::SearchPreloadShareableCacheType::kEnabled:
      return true;
    case features::SearchPreloadShareableCacheType::kDisabled:
      return false;
  }
}

}  // namespace prerender_utils
