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

}  // namespace prerender_utils
