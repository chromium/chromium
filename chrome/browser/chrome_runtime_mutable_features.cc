// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file only contains the definition of
// ChromeBrowserFieldTrials::EnableRuntimeMutableFeatures(). It lives in its own
// file (rather than in chrome_browser_field_trials.cc) so that it can have
// dedicated OWNERS. See
// //components/variations/RUNTIME_MUTABLE_FEATURES_OWNERS.

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/performance_manager/public/features.h"
#include "components/variations/platform_field_trials.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

void ChromeBrowserFieldTrials::EnableRuntimeMutableFeatures(
    base::FeatureList* feature_list) {
  variations::PlatformFieldTrials::EnableRuntimeMutableFeatures(feature_list);

  // Add calls to enable runtime-mutable features here.
#if !BUILDFLAG(IS_ANDROID)
  // Defined on desktop only (see password_features.h).
  feature_list->EnableRuntimeMutability(
      password_manager::features::kPasskeyUnlockPromo, base::DoNothing());
#endif  // !BUILDFLAG(IS_ANDROID)
  feature_list->EnableRuntimeMutability(
      performance_manager::features::kPMWithheldFromViewVoter,
      base::DoNothing());
  feature_list->EnableRuntimeMutability(
      performance_manager::features::kPMSidePanelLoadingVoter,
      base::DoNothing());
#if BUILDFLAG(IS_ANDROID)
  feature_list->EnableRuntimeMutability(chrome::android::kAvoidTaskTrampolines,
                                        base::DoNothing());
#endif  // BUILDFLAG(IS_ANDROID)
}
