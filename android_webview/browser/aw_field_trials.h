// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_

#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "components/variations/platform_field_trials.h"

namespace internal {

// Utility class for overriding feature states on a base::FeatureList.
// Exposed via the internal namespace for tests.
class AwFeatureOverrides {
 public:
  explicit AwFeatureOverrides(base::FeatureList& feature_list);

  AwFeatureOverrides(const AwFeatureOverrides& other) = delete;
  AwFeatureOverrides& operator=(const AwFeatureOverrides& other) = delete;

  ~AwFeatureOverrides();

  // Enables a feature with WebView-specific override.
  void EnableFeature(const base::Feature& feature);

  // Disables a feature with WebView-specific override.
  void DisableFeature(const base::Feature& feature);

  // Enables or disable a feature with a field trial. This can be used for
  // setting feature parameters.
  void OverrideFeatureWithFieldTrial(
      const base::Feature& feature,
      base::FeatureList::OverrideState override_state,
      base::FieldTrial* field_trial);

 private:
  struct FieldTrialOverride {
    raw_ref<const base::Feature> feature;
    base::FeatureList::OverrideState override_state;
    raw_ptr<base::FieldTrial> field_trial;
  };

  raw_ref<base::FeatureList> feature_list_;
  std::vector<base::FeatureList::FeatureOverrideInfo> overrides_;
  std::vector<FieldTrialOverride> field_trial_overrides_;
};

}  // namespace internal

// Responsible for setting up field trials specific to WebView. Used to provide
// WebView-specific defaults that are used over the state coming from the
// base::Feature when there is no other (e.g. server-side) override.
// Lifetime: Singleton
class AwFieldTrials : public variations::PlatformFieldTrials {
 public:
  AwFieldTrials() = default;

  AwFieldTrials(const AwFieldTrials&) = delete;
  AwFieldTrials& operator=(const AwFieldTrials&) = delete;

  ~AwFieldTrials() override = default;

  // variations::PlatformFieldTrials:
  void OnVariationsSetupComplete() override;
  void RegisterFeatureOverrides(base::FeatureList* feature_list) override;
};

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
