// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserFrameworkEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserFramework);
    EXPECT_TRUE(IsDefaultBrowserFrameworkEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserFramework);
    EXPECT_FALSE(IsDefaultBrowserFrameworkEnabled());
  }
}

}  // namespace default_browser
