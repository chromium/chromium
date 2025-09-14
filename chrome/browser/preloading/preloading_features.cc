// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_features.h"

namespace features {

BASE_FEATURE(kPrewarm, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string, kPrewarmUrl, &kPrewarm, "url", "");
BASE_FEATURE_PARAM(bool,
                   kPrewarmZeroSuggestTrigger,
                   &kPrewarm,
                   "zero_suggest_trigger",
                   false);
BASE_FEATURE_PARAM(bool,
                   kForceEnableWithDevTools,
                   &kPrewarm,
                   "force_enable_with_devtools",
                   false);

BASE_FEATURE(kPrerender2WarmUpCompositorForBookmarkBar,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPrerender2WarmUpCompositorForNewTabPage,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
