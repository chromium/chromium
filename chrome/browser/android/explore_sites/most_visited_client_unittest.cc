// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/most_visited_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace explore_sites {

TEST(MostVisitedClientTest, ReturnsNullWhenExploreSitesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(chrome::android::kExploreSites);
  EXPECT_EQ(nullptr, MostVisitedClient::Create());
}

TEST(MostVisitedClientTest, ReturnsNonNullWhenExploreSitesEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(chrome::android::kExploreSites);
  EXPECT_NE(nullptr, MostVisitedClient::Create());
}

}  // namespace explore_sites
