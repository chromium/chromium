// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_

#include "base/feature_list.h"
#include "components/variations/platform_field_trials.h"

// Responsible for setting up feature overrides specific to WebView.
//
// Both Chrome and WebView share the IS_ANDROID build flag, which, in absence of
// an IS_WEBVIEW build flag, creates a difficulty for features whose default
// enabled state should be different between them. RegisterFeatureOverrides()
// provides a solution for this, where a different state can be configured
// specifically for WebView.
//
// These feature overrides only apply to the default base::Feature states, but
// are by no means final. The feature state resolution steps are:
//
// 1. The base::Feature's default state
// 2. The WebView-specific default state set in RegisterFeatureOverrides()
// 3. Any changes imposed by experimentation in active Finch studies
// 4. Any changes imposed by user configuration in the WebView DevUI
//
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
