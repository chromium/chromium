// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "components/variations/platform_field_trials.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/variations_associated_data.h"
#endif

namespace base {
class FeatureList;
}

class ChromeBrowserFieldTrials : public variations::PlatformFieldTrials {
 public:
  ChromeBrowserFieldTrials();

  ChromeBrowserFieldTrials(const ChromeBrowserFieldTrials&) = delete;
  ChromeBrowserFieldTrials& operator=(const ChromeBrowserFieldTrials&) = delete;

  ~ChromeBrowserFieldTrials() override;

  // Registers synthetic field trials. Call after `g_browser_process` exists.
  static void RegisterSyntheticTrials();

  // variations::PlatformFieldTrials:
  void OnVariationsSetupComplete() override;
  void SetUpClientSideFieldTrials(
      bool has_seed,
      const variations::EntropyProviders& entropy_providers,
      base::FeatureList* feature_list) override;
  void RegisterFeatureOverrides(base::FeatureList* feature_list) override;
  void EnableRuntimeMutableFeatures(base::FeatureList* feature_list) override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
