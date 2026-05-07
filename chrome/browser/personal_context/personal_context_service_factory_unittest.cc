// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/personal_context_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {

class PersonalContextServiceFactoryTest : public testing::Test {
 public:
  PersonalContextServiceFactoryTest() = default;
  ~PersonalContextServiceFactoryTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PersonalContextServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  TestingProfile profile;
  EXPECT_NE(nullptr, PersonalContextServiceFactory::GetForProfile(&profile));
}

TEST_F(PersonalContextServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      personal_context::features::kPersonalContext);
  TestingProfile profile;
  EXPECT_EQ(nullptr, PersonalContextServiceFactory::GetForProfile(&profile));
}

TEST_F(PersonalContextServiceFactoryTest,
       CreatesNoServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, PersonalContextServiceFactory::GetForProfile(otr_profile));
}

}  // namespace personal_context
