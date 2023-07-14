// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_provider_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_provider.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class ScopedInitFeature {
 public:
  ScopedInitFeature(const base::Feature& feature, bool enable) {
    feature_list_.InitWithFeatureState(feature, enable);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
}  // namespace

class IpProtectionAuthTokenProviderFactoryTest : public testing::Test {
 protected:
  explicit IpProtectionAuthTokenProviderFactoryTest(bool feature_enabled = true)
      // Note that the order of initialization is important here - we want to
      // set the value of the feature before anything else since it's used by
      // the `IpProtectionAuthTokenProviderFactory` logic.
      : scoped_feature_(net::features::kEnableIpProtectionProxy,
                        feature_enabled),
        profile_selections_(IpProtectionAuthTokenProviderFactory::GetInstance(),
                            IpProtectionAuthTokenProviderFactory::
                                CreateProfileSelectionsForTesting()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  ScopedInitFeature scoped_feature_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      profile_selections_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(IpProtectionAuthTokenProviderFactoryTest,
       ServiceCreationSucceedsWhenFlagEnabled) {
  IpProtectionAuthTokenProvider* service =
      IpProtectionAuthTokenProviderFactory::GetForProfile(profile());
  ASSERT_TRUE(service);
  service->Shutdown();
}

TEST_F(IpProtectionAuthTokenProviderFactoryTest, OtrProfileUsesPrimaryProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // The regular profile and the off-the-record profile must be different.
  ASSERT_NE(profile(), otr_profile);

  // The same `IpProtectionAuthTokenProvider` should be used for both the main
  // profile and the corresponding OTR profile.
  EXPECT_EQ(IpProtectionAuthTokenProviderFactory::GetForProfile(profile()),
            IpProtectionAuthTokenProviderFactory::GetForProfile(otr_profile));
}

class IpProtectionAuthTokenProviderFactoryFeatureDisabledTest
    : public IpProtectionAuthTokenProviderFactoryTest {
 public:
  IpProtectionAuthTokenProviderFactoryFeatureDisabledTest()
      : IpProtectionAuthTokenProviderFactoryTest(/*feature_enabled=*/false) {}
};

TEST_F(IpProtectionAuthTokenProviderFactoryFeatureDisabledTest,
       ServiceCreationFailsWhenFlagDisabled) {
  IpProtectionAuthTokenProvider* service =
      IpProtectionAuthTokenProviderFactory::GetForProfile(profile());
  ASSERT_FALSE(service);
}
