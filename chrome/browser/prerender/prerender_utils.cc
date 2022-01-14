// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_utils.h"

#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/features.h"

namespace prerender_utils {

const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";

bool IsDirectUrlInputPrerenderEnabled() {
  return blink::features::IsPrerender2Enabled() &&
         base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2);
}

bool IsSearchSuggestionPrerenderEnabled() {
  // This is a tentative flag combination. Currently we haven't decided how to
  // run the two experiments of DirectUrlInput Prerender and SearchSuggestion
  // Prerender parallelly, so we simply make the feature flag of
  // kSupportSearchSuggestionForPrerender2 a FeatureParam of
  // kOmniboxTriggerForPrerender2 to reuse the existing feature structure.
  // TODO(https://crbug.com/1278634): Update the restriction after we finalize
  // the experiment design.
  return blink::features::IsPrerender2Enabled() &&
         base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2) &&
         features::kSupportSearchSuggestionForPrerender2.Get();
}

}  // namespace prerender_utils
