// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::test::FeatureRef;
using base::test::ScopedFeatureList;
using sync_preferences::TestingPrefServiceSyncable;
using ::testing::TestWithParam;
using ::testing::Values;

std::unique_ptr<KeyedService> CreateFakeUnexportableKeyService(
    content::BrowserContext* context) {
  return std::make_unique<unexportable_keys::FakeUnexportableKeyService>();
}

bool DoesServiceExistForProfile(Profile* profile) {
  return BoundSessionCookieRefreshServiceFactory::GetForProfile(profile) !=
         nullptr;
}

struct BoundSessionCookieRefreshServiceFactoryTestParams {
  std::string test_name;
  std::vector<FeatureRef> enabled_features;
  std::vector<FeatureRef> disabled_features;
  ~BoundSessionCookieRefreshServiceFactoryTestParams() = default;
};

const BoundSessionCookieRefreshServiceFactoryTestParams kTestCases[] = {
    {"OnlyBoundSessionCredentialsEnabled",
     {switches::kEnableBoundSessionCredentials},
     {kEnableBoundSessionCredentialsOnDiceProfiles}},
    {"OnlyEnableBoundSessionCredentialsOnDiceProfilesEnabled",
     {kEnableBoundSessionCredentialsOnDiceProfiles},
     {switches::kEnableBoundSessionCredentials}},
    {"AllEnabled",
     {switches::kEnableBoundSessionCredentials,
      kEnableBoundSessionCredentialsOnDiceProfiles},
     {}},
    {"AllDisabled",
     {},
     {switches::kEnableBoundSessionCredentials,
      kEnableBoundSessionCredentialsOnDiceProfiles}}};

class BoundSessionCookieRefreshServiceFactoryTest
    : public TestWithParam<BoundSessionCookieRefreshServiceFactoryTestParams> {
 public:
  BoundSessionCookieRefreshServiceFactoryTest() {
    feature_list.InitWithFeatures(GetParam().enabled_features,
                                  GetParam().disabled_features);
  }

  void CreateProfile(bool otr_profile = false) {
    // `BoundSessionCookieRefreshService` depends on `UnexportableKeyService`,
    // ensure it is not null.
    profile_builder_.AddTestingFactory(
        UnexportableKeyServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeUnexportableKeyService));

    if (otr_profile) {
      original_profile_ = std::make_unique<TestingProfile>();
      otr_profile_ =
          std::move(profile_builder_.BuildIncognito(original_profile_.get()));
    } else {
      original_profile_ = profile_builder_.Build();
    }
  }

  bool ShouldServiceExistDiceEnabled() {
    return base::Contains(GetParam().enabled_features,
                          switches::kEnableBoundSessionCredentials) &&
           base::Contains(GetParam().enabled_features,
                          kEnableBoundSessionCredentialsOnDiceProfiles);
  }

  bool ShouldServiceExistAccountConsistencyDisabled() {
    return base::Contains(GetParam().enabled_features,
                          switches::kEnableBoundSessionCredentials);
  }

  TestingProfile* original_profile() { return original_profile_.get(); }
  TestingProfile* otr_profile() { return otr_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment;
  ScopedFeatureList feature_list;
  TestingProfile::Builder profile_builder_;
  std::unique_ptr<TestingProfile> original_profile_;
  raw_ptr<TestingProfile> otr_profile_;
};

TEST_P(BoundSessionCookieRefreshServiceFactoryTest, RegularProfileDiceEnabled) {
  CreateProfile();
  ASSERT_FALSE(original_profile()->IsOffTheRecord());
  ASSERT_EQ(
      AccountConsistencyModeManager::GetMethodForProfile(original_profile()),
      signin::AccountConsistencyMethod::kDice);

  EXPECT_EQ(ShouldServiceExistDiceEnabled(),
            DoesServiceExistForProfile(original_profile()));
}

TEST_P(BoundSessionCookieRefreshServiceFactoryTest,
       RegularProfileDiceDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "allow-browser-signin", "false");
  CreateProfile();
  ASSERT_FALSE(original_profile()->IsOffTheRecord());
  ASSERT_EQ(
      AccountConsistencyModeManager::GetMethodForProfile(original_profile()),
      signin::AccountConsistencyMethod::kDisabled);
  EXPECT_EQ(ShouldServiceExistAccountConsistencyDisabled(),
            DoesServiceExistForProfile(original_profile()));
}

TEST_P(BoundSessionCookieRefreshServiceFactoryTest, OTRProfile) {
  CreateProfile(/*otr_profile=*/true);
  ASSERT_TRUE(otr_profile()->IsOffTheRecord());
  ASSERT_EQ(AccountConsistencyModeManager::GetMethodForProfile(otr_profile()),
            signin::AccountConsistencyMethod::kDisabled);
  EXPECT_EQ(ShouldServiceExistAccountConsistencyDisabled(),
            DoesServiceExistForProfile(otr_profile()));
}

TEST(BoundSessionCookieRefreshServiceFactoryTestNullUnexportableKeyService,
     NullUnexportableKeyService) {
  content::BrowserTaskEnvironment task_environment;
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({switches::kEnableBoundSessionCredentials,
                                 kEnableBoundSessionCredentialsOnDiceProfiles},
                                {});

  BrowserContextKeyedServiceFactory::TestingFactory
      unexportable_key_service_factory = base::BindRepeating(
          [](content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> { return nullptr; });
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      UnexportableKeyServiceFactory::GetInstance(),
      std::move(unexportable_key_service_factory));

  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  ASSERT_FALSE(UnexportableKeyServiceFactory::GetForProfile(profile.get()));
  EXPECT_FALSE(DoesServiceExistForProfile(profile.get()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    BoundSessionCookieRefreshServiceFactoryTest,
    ::testing::ValuesIn<BoundSessionCookieRefreshServiceFactoryTestParams>(
        kTestCases),
    [](const testing::TestParamInfo<
        BoundSessionCookieRefreshServiceFactoryTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace
