// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"

#include "chrome/browser/preloading/preloading_features.h"

namespace test {

ScopedPrewarmFeatureList::ScopedPrewarmFeatureList(PrewarmState state) {
  switch (state) {
    case PrewarmState::kDisabled:
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kPrewarm,
                                 features::kPrewarmZeroSuggestTrigger});
      break;
    case PrewarmState::kEnabledWithNoTrigger:
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kPrewarm,
            {
                {"url", "https://search.example.com/prewarm.html"},
                {"throttle_prefetch", "true"},
                {"revalidate", "true"},
                {"throttle_user_navigation", "true"},
            }}},
          /*disabled_features=*/{features::kPrewarmZeroSuggestTrigger});
      break;
    case PrewarmState::kEnabledWithDefaultTrigger:
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kPrewarm,
            {
                {"url", "https://search.example.com/prewarm.html"},
                {"throttle_prefetch", "true"},
                {"revalidate", "true"},
                {"throttle_user_navigation", "true"},
            }},
           {features::kPrewarmZeroSuggestTrigger, {}}},
          /*disabled_features=*/{});
      break;
    case PrewarmState::kEnabledWithInterationTrigger:
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kPrewarm,
            {
                {"url", "https://search.example.com/prewarm.html"},
                {"throttle_prefetch", "true"},
                {"revalidate", "true"},
                {"throttle_user_navigation", "true"},
            }}},
          /*disabled_features=*/{features::kPrewarmZeroSuggestTrigger});
      break;
  }
}

}  // namespace test
