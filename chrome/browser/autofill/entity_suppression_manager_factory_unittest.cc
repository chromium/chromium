// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/entity_suppression_manager_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class EntitySuppressionManagerFactoryTest : public testing::Test {
 public:
  EntitySuppressionManagerFactoryTest() = default;
  ~EntitySuppressionManagerFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the factory returns nullptr when the feature flag is disabled.
TEST_F(EntitySuppressionManagerFactoryTest, ReturnsNullptrWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAmbientAutofillSuppression);
  TestingProfile profile;

  EXPECT_EQ(nullptr, EntitySuppressionManagerFactory::GetForProfile(&profile));
}

// Tests that the factory creates an EntitySuppressionManager instance for
// regular profiles when the feature flag is enabled.
TEST_F(EntitySuppressionManagerFactoryTest,
       CreatesServiceForRegularProfileWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillAmbientAutofillSuppression);
  TestingProfile profile;

  EntitySuppressionManager* service =
      EntitySuppressionManagerFactory::GetForProfile(&profile);

  EXPECT_NE(nullptr, service);
}

// Tests that off-the-record profiles return nullptr even when feature is
// enabled.
TEST_F(EntitySuppressionManagerFactoryTest, ReturnsNullptrForIncognitoProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillAmbientAutofillSuppression);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  EXPECT_EQ(nullptr,
            EntitySuppressionManagerFactory::GetForProfile(otr_profile));
}

}  // namespace autofill
