// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace first_party_sets {

class FirstPartySetsPolicyServiceFactoryTest : public testing::Test {
 public:
  FirstPartySetsPolicyServiceFactoryTest() = default;

 private:
  content::BrowserTaskEnvironment env_;
};

TEST_F(FirstPartySetsPolicyServiceFactoryTest, DisabledForGuestProfiles) {
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(FirstPartySetsPolicyServiceFactory::GetPolicyIfEnabled(*profile),
            nullptr);
}

TEST_F(FirstPartySetsPolicyServiceFactoryTest, DisabledByFeature) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kFirstPartySets);
  TestingProfile profile;

  EXPECT_EQ(FirstPartySetsPolicyServiceFactory::GetPolicyIfEnabled(profile),
            nullptr);
}

TEST_F(FirstPartySetsPolicyServiceFactoryTest, DisabledByPolicy) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile profile;

  profile.GetPrefs()->SetBoolean(first_party_sets::kFirstPartySetsEnabled,
                                 false);
  EXPECT_EQ(FirstPartySetsPolicyServiceFactory::GetPolicyIfEnabled(profile),
            nullptr);
}

TEST_F(FirstPartySetsPolicyServiceFactoryTest, EnabledWithPolicy) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile profile;

  base::Value empty_lists = base::JSONReader::Read(R"(
             {
                "replacements": [],
                "additions": []
              }
            )")
                                .value();
  base::Value expected_policy = empty_lists.Clone();
  profile.GetPrefs()->SetBoolean(first_party_sets::kFirstPartySetsEnabled,
                                 true);
  profile.GetPrefs()->SetDict(first_party_sets::kFirstPartySetsOverrides,
                              std::move(empty_lists.GetDict()));

  const base::Value::Dict* policy =
      FirstPartySetsPolicyServiceFactory::GetPolicyIfEnabled(profile);
  ASSERT_NE(policy, nullptr);
  EXPECT_TRUE(*policy == expected_policy.GetDict());
}

TEST_F(FirstPartySetsPolicyServiceFactoryTest,
       OffTheRecordProfile_SameServiceAsOriginalProfile) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile profile;

  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile);

  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service,
            FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
                profile.GetOffTheRecordProfile(otr_profile_id,
                                               /*create_if_needed=*/true)));
}

}  // namespace first_party_sets
