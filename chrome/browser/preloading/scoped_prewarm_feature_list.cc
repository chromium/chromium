// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"

#include "chrome/browser/preloading/preloading_features.h"

namespace test {

ScopedPrewarmFeatureList::ScopedPrewarmFeatureList(PrewarmState state) {
  switch (state) {
    case PrewarmState::kDisabled:
      scoped_feature_list_.InitAndDisableFeature(features::kPrewarm);
      break;
    case PrewarmState::kEnabledWithNoTrigger:
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPrewarm,
          {{"url", "https://search.example.com/prewarm.html"}});
      break;
    case PrewarmState::kEnabledWithDefaultTrigger:
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kPrewarm,
          {{"url", "https://search.example.com/prewarm.html"},
           {"zero_suggest_trigger", "true"}});
      break;
  }
}

}  // namespace test
