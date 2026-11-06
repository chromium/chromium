// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/personal_context_service_factory.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "chrome/browser/personal_context/personal_context_eligibility_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {

class PersonalContextServiceFactoryTest : public testing::Test {
 public:
  PersonalContextServiceFactoryTest() = default;
  ~PersonalContextServiceFactoryTest() override = default;

 protected:
  std::unique_ptr<TestingProfile> BuildProfile() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        PersonalContextServiceFactory::GetInstance(),
        PersonalContextServiceFactory::GetDefaultFactory());
    return builder.Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PersonalContextServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  EXPECT_NE(nullptr,
            PersonalContextServiceFactory::GetForProfile(profile.get()));
}

TEST_F(PersonalContextServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  EXPECT_EQ(nullptr,
            PersonalContextServiceFactory::GetForProfile(profile.get()));
}

TEST_F(PersonalContextServiceFactoryTest,
       CreatesNoServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      personal_context::features::kPersonalContext);
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  Profile* otr_profile = profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, PersonalContextServiceFactory::GetForProfile(otr_profile));
}

class PersonalContextEligibilityServiceFactoryTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPersonalContext};
  content::BrowserTaskEnvironment task_environment_;
};

// A consumer profile isn't made ineligible because of its provider.
TEST_F(PersonalContextEligibilityServiceFactoryTest, ConsumerProvider) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ASSERT_TRUE(glic::GetGlicProviderSpec(profile.get())
                  .SupportsFeature(glic::GlicFeature::kPersonalContext));

  PersonalContextEligibilityService* service =
      PersonalContextEligibilityServiceFactory::GetForProfile(profile.get());
  ASSERT_TRUE(service);
  EXPECT_NE(service->GetNonEligibilityReason(),
            std::make_optional(
                PersonalContextNonEligibilityReason::kUnsupportedGlicProvider));
}

class PersonalContextEligibilityServiceFactoryGeminiEnterpriseTest
    : public PersonalContextEligibilityServiceFactoryTest {
 private:
  glic::ScopedGlicProviderOverride provider_override_{
      glic::GlicProvider::kGeminiEnterprise};
};

// A profile latched to Gemini Enterprise isn't eligible for Personal Context.
TEST_F(PersonalContextEligibilityServiceFactoryGeminiEnterpriseTest,
       GeminiEnterpriseProviderIsIneligible) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  ASSERT_EQ(glic::GetGlicProviderSpec(profile.get()).GetSettingsSectionId(),
            glic::kGeminiEnterpriseSettingsSectionId);

  PersonalContextEligibilityService* service =
      PersonalContextEligibilityServiceFactory::GetForProfile(profile.get());
  ASSERT_TRUE(service);
  EXPECT_EQ(service->GetEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
  EXPECT_EQ(service->GetNonEligibilityReason(),
            PersonalContextNonEligibilityReason::kUnsupportedGlicProvider);
}

}  // namespace personal_context
