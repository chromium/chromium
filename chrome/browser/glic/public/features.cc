// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"

#include "build/android_buildflags.h"
#include "build/build_config.h"

namespace features {

BASE_FEATURE(kGlicTabRestoration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicChromeStatusIcon, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicChromeStatusIconSizePx{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-size-px", 20};

BASE_FEATURE(kGlicOrphanedReattachment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSelectionPrompt, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kGlicSelectionPromptUseWidget{
    &kGlicSelectionPrompt, "use_widget", true};

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutoOpenGlicForPdfWithOnboarding({
    &kAutoOpenGlicForPdf,
    "AutoOpenGlicForPdfWithOnboarding",
    false,
});

BASE_FEATURE(kGlicInvoke, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicCreateTabAdjacent, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicDefaultToLastActiveConversation,
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<base::TimeDelta>
    kGlicDefaultToLastActiveConversationMaxRecency{
        &kGlicDefaultToLastActiveConversation, "max_recency",
        base::Minutes(20)};

BASE_FEATURE(kGlicSummarizeVideoSuggestion, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
