// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class MultistepFilterServiceFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

 protected:
  void SignIn() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()), "test@gmail.com",
        signin::ConsentLevel::kSignin);
  }
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(MultistepFilterServiceFactoryTest,
       ServiceCreatedForSignedInUserWithFeatureEnabled) {
  feature_list_.InitAndEnableFeature(kMultistepFilter);
  SignIn();
  auto* service = MultistepFilterServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(MultistepFilterServiceFactoryTest,
       ServiceNotCreatedWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(kMultistepFilter);
  SignIn();
  auto* service = MultistepFilterServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(MultistepFilterServiceFactoryTest, ServiceCreatedWhenUserNotSignedIn) {
  feature_list_.InitAndEnableFeature(kMultistepFilter);
  auto* service = MultistepFilterServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(MultistepFilterServiceFactoryTest,
       ServiceNotCreatedForIncognitoProfile) {
  feature_list_.InitAndEnableFeature(kMultistepFilter);
  SignIn();
  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto* service =
      MultistepFilterServiceFactory::GetForProfile(incognito_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(MultistepFilterServiceFactoryTest, ServiceNotCreatedForGuestProfile) {
  feature_list_.InitAndEnableFeature(kMultistepFilter);
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();

  auto* service =
      MultistepFilterServiceFactory::GetForProfile(guest_profile.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(MultistepFilterServiceFactoryTest, RegisterProfilePrefs) {
  const PrefService::Preference* impressions_pref =
      profile_->GetPrefs()->FindPreference(kRetentionSuggestionImpressionsPref);
  ASSERT_NE(impressions_pref, nullptr);
  EXPECT_TRUE(impressions_pref->IsDefaultValue());

  const PrefService::Preference* acceptances_pref =
      profile_->GetPrefs()->FindPreference(kRetentionSuggestionAcceptancesPref);
  ASSERT_NE(acceptances_pref, nullptr);
  EXPECT_TRUE(acceptances_pref->IsDefaultValue());

  const PrefService::Preference* last_accepted_pref =
      profile_->GetPrefs()->FindPreference(
          kRetentionIsLastSuggestionAcceptedPref);
  ASSERT_NE(last_accepted_pref, nullptr);
  EXPECT_TRUE(last_accepted_pref->IsDefaultValue());
}

}  // namespace multistep_filter
