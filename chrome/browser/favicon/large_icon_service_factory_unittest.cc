// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/large_icon_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class LargeIconServiceFactoryTest : public testing::Test {};

TEST_F(LargeIconServiceFactoryTest, LargeFaviconFromGoogleDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kLargeFaviconFromGoogle);

#if BUILDFLAG(IS_ANDROID)
  const int expected = 24;
#else
  const int expected = 16;
#endif

  EXPECT_EQ(LargeIconServiceFactory::desired_size_in_dip_for_server_requests(),
            expected);
}

TEST_F(LargeIconServiceFactoryTest, LargeFaviconFromGoogleEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kLargeFaviconFromGoogle,
                                              {{"favicon_size_in_dip", "256"}});

  EXPECT_EQ(LargeIconServiceFactory::desired_size_in_dip_for_server_requests(),
            256);
}
