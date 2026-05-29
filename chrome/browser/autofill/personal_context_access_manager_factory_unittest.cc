// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_context_access_manager_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalContextAccessManagerFactoryTest : public testing::Test {
 public:
  PersonalContextAccessManagerFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAmbientAutofill,
         personal_context::features::kPersonalContext},
        {});
  }
  ~PersonalContextAccessManagerFactoryTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

// Test that the factory successfully instantiates a
// PersonalContextAccessManager when the ambient autofill feature is enabled.
TEST_F(PersonalContextAccessManagerFactoryTest, CreatesService) {
  TestingProfile profile;
  EXPECT_NE(nullptr,
            PersonalContextAccessManagerFactory::GetForProfile(&profile));
}

// Test that the factory returns nullptr (does not instantiate the service)
// when using an Incognito / Off-the-record profile.
TEST_F(PersonalContextAccessManagerFactoryTest, CreatesNoServiceForIncognito) {
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            PersonalContextAccessManagerFactory::GetForProfile(otr_profile));
}

}  // namespace autofill
