// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_

#include "base/macros.h"
#include "components/variations/platform_field_trials.h"

// Responsible for setting up field trials specific to WebView. Currently all
// functions are stubs, as WebView has no specific field trials.
class AwFieldTrials : public variations::PlatformFieldTrials {
 public:
  AwFieldTrials() = default;
  ~AwFieldTrials() override = default;

  // variations::PlatformFieldTrials:
  void SetupFieldTrials() override;
  void SetupFeatureControllingFieldTrials(
      bool has_seed,
      const base::FieldTrial::EntropyProvider& low_entropy_provider,
      base::FeatureList* feature_list) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AwFieldTrials);
};

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FIELD_TRIALS_H_
