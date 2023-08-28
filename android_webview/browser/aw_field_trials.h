// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_

#include "components/variations/platform_field_trials.h"

// Responsible for setting up field trials specific to WebView. Currently all
// functions are stubs, as WebView has no specific field trials.
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
