// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserFrameworkEnabled) {
  EXPECT_FALSE(IsDefaultBrowserFrameworkEnabled());
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

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserPromptSurfacesEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserPromptSurfaces);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserPromptSurfacesEnabled());
#else
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserPromptSurfaces);
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
}

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserChangedOsNotificationEnabled) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserChangedOsNotification);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserChangedOsNotificationEnabled());
#else
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserChangedOsNotification);
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
  }
}

}  // namespace default_browser
