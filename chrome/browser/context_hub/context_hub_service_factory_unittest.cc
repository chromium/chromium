// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {

class ContextHubServiceFactoryTest : public testing::Test {
 public:
  ContextHubServiceFactoryTest() = default;
  ~ContextHubServiceFactoryTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ContextHubServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kContextHub);
  TestingProfile profile;
  EXPECT_NE(nullptr, ContextHubServiceFactory::GetForProfile(&profile));
}

TEST_F(ContextHubServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kContextHub);
  TestingProfile profile;
  EXPECT_EQ(nullptr, ContextHubServiceFactory::GetForProfile(&profile));
}

TEST_F(ContextHubServiceFactoryTest,
       DoesNotCreateServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kContextHub);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, ContextHubServiceFactory::GetForProfile(otr_profile));
}

}  // namespace context_hub
