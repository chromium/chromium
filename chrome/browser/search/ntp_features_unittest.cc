// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include <map>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_features {

TEST(NTPFeaturesTest, IsRealboxEnabled) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_FALSE(IsRealboxEnabled());

  feature_list.InitAndEnableFeature(kRealbox);
  EXPECT_TRUE(IsRealboxEnabled());

  feature_list.Reset();
  EXPECT_FALSE(IsRealboxEnabled());

  feature_list.InitAndEnableFeature(omnibox::kZeroSuggestionsOnNTPRealbox);
  EXPECT_TRUE(IsRealboxEnabled());

  feature_list.Reset();
  EXPECT_FALSE(IsRealboxEnabled());

  // zero-prefix suggestions are configured for the NTP Omnibox.
  feature_list.InitWithFeaturesAndParameters(
      {{omnibox::kOnFocusSuggestions,
        {{"ZeroSuggestVariant:7:*", "Does not matter"}}}},
      {});
  EXPECT_FALSE(IsRealboxEnabled());

  feature_list.Reset();
  EXPECT_FALSE(IsRealboxEnabled());

  // zero-prefix suggestions are configured for the NTP Realbox.
  feature_list.InitWithFeaturesAndParameters(
      {{omnibox::kOnFocusSuggestions,
        {{"ZeroSuggestVariant:15:*", "Does not matter"}}}},
      {});
  EXPECT_TRUE(IsRealboxEnabled());
}

}  // namespace ntp_features
