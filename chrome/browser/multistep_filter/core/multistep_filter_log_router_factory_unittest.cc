// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class MultistepFilterLogRouterFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MultistepFilterLogRouterFactoryTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMultistepFilter);

  TestingProfile profile;
  EXPECT_EQ(nullptr, MultistepFilterLogRouterFactory::GetForProfile(&profile));
}

TEST_F(MultistepFilterLogRouterFactoryTest, IncognitoProfileReturnsNull) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMultistepFilter);

  TestingProfile profile;
  EXPECT_EQ(nullptr,
            MultistepFilterLogRouterFactory::GetForProfile(
                profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(MultistepFilterLogRouterFactoryTest, FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMultistepFilter);

  TestingProfile profile;
  MultistepFilterLogRouterImpl* service =
      MultistepFilterLogRouterFactory::GetForProfile(&profile);

  EXPECT_NE(nullptr, service);

  // Verify that the same instance is returned for the same profile.
  EXPECT_EQ(service, MultistepFilterLogRouterFactory::GetForProfile(&profile));
}

}  // namespace multistep_filter
