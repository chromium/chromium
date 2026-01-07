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
                   kPrewarmUserInteractionTrigger,
                   &kPrewarm,
                   "user_interaction_trigger",
                   false);
BASE_FEATURE_PARAM(bool,
                   kForceEnableWithDevTools,
                   &kPrewarm,
                   "force_enable_with_devtools",
                   false);

// On Android, we limit minimum memory threshold for DSEPrewarm feature due to
// Android OOM killer eagerly killing the renderer. Since the Prerender2 has
// the minimum memory setting of 2GB, we should set it to a value greater than
// 2GB, since DSEPrewarm uses Prerender2 feature as well.
static constexpr int kDSEPrearmDefaultMemoryThresholdMb =
#if BUILDFLAG(IS_ANDROID)
    1700;
#else
    0;
#endif
BASE_FEATURE_PARAM(int,
                   kMinMemoryThresholdMb,
                   &kPrewarm,
                   "min_memory_threshold_mb",
                   kDSEPrearmDefaultMemoryThresholdMb);

BASE_FEATURE(kPrerender2WarmUpCompositorForBookmarkBar,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPrerender2WarmUpCompositorForNewTabPage,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
