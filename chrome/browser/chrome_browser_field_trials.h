// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/memory/raw_ptr.h"
#include "components/variations/platform_field_trials.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/variations_associated_data.h"
#endif

class PrefService;

namespace base {
class FeatureList;
}

class ChromeBrowserFieldTrials : public variations::PlatformFieldTrials {
 public:
  explicit ChromeBrowserFieldTrials(PrefService* local_state);

  ChromeBrowserFieldTrials(const ChromeBrowserFieldTrials&) = delete;
  ChromeBrowserFieldTrials& operator=(const ChromeBrowserFieldTrials&) = delete;

  ~ChromeBrowserFieldTrials() override;

  // variations::PlatformFieldTrials:
  void SetUpClientSideFieldTrials(
      bool has_seed,
      const variations::EntropyProviders& entropy_providers,
      base::FeatureList* feature_list) override;
  void RegisterSyntheticTrials() override;
#if BUILDFLAG(IS_LINUX)
  void RegisterFeatureOverrides(base::FeatureList* feature_list) override;
#endif

 private:
  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService, AcrossTasksDanglingUntriaged> local_state_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
