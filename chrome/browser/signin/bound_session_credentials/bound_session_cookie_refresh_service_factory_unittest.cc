// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/fake_keyed_unexportable_key_service.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::test::FeatureRef;
using base::test::FeatureRefAndParams;
using base::test::ScopedFeatureList;
using sync_preferences::TestingPrefServiceSyncable;
using ::testing::TestWithParam;
using ::testing::Values;

std::unique_ptr<KeyedService> CreateFakeUnexportableKeyService(
    content::BrowserContext* context) {
  return std::make_unique<FakeKeyedUnexportableKeyService>();
}

bool DoesServiceExistForProfile(Profile* profile) {
  return BoundSessionCookieRefreshServiceFactory::GetForProfile(profile) !=
         nullptr;
}

struct BoundSessionCookieRefreshServiceFactoryTestParams {
  std::string test_name;
  std::vector<FeatureRefAndParams> enabled_features;
  std::vector<FeatureRef> disabled_features;
  std::optional<bool> feature_policy_value;
  std::optional<switches::EnableBoundSessionCredentialsDiceSupport>
      expected_support;  // std::nullopt means no support at all.
  ~BoundSessionCookieRefreshServiceFactoryTestParams() = default;
};

const BoundSessionCookieRefreshServiceFactoryTestParams kTestCases[] = {
    {
        "EnabledWithDefaultDiceSupport",
        {{switches::kEnableBoundSessionCredentials, {}}},
        {},
        std::nullopt,
        switches::EnableBoundSessionCredentialsDiceSupport::kEnabled,
    },
    {
        "EnabledForNonDiceProfiles",
        {{switches::kEnableBoundSessionCredentials,
          {{"dice-support", "disabled"}}}},
        {},
        std::nullopt,
        switches::EnableBoundSessionCredentialsDiceSupport::kDisabled,
    },
    {
        "EnabledForAllProfiles",
        {{switches::kEnableBoundSessionCredentials,
          {{"dice-support", "enabled"}}}},
        {},
        std::nullopt,
        switches::EnableBoundSessionCredentialsDiceSupport::kEnabled,
    },
    {
        "Disabled",
        {},
        {switches::kEnableBoundSessionCredentials},
        std::nullopt,
        std::nullopt,
    },
    {
        "DisabledByPolicy",
        {{switches::kEnableBoundSessionCredentials, {}}},
        {},
        false,
        std::nullopt,
    },
    {
        "EnabledByPolicy",
        {},
        {switches::kEnableBoundSessionCredentials},
        true,
        switches::EnableBoundSessionCredentialsDiceSupport::kEnabled,
    },
};

class BoundSessionCookieRefreshServiceFactoryTest
    : public TestWithParam<BoundSessionCookieRefreshServiceFactoryTestParams> {
 public:
  BoundSessionCookieRefreshServiceFactoryTest() {
    feature_list.InitWithFeaturesAndParameters(GetParam().enabled_features,
                                               GetParam().disabled_features);
  }

  void CreateProfile(bool otr_profile = false) {
    // `BoundSessionCookieRefreshService` depends on `UnexportableKeyService`,
    // ensure it is not null.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        UnexportableKeyServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeUnexportableKeyService));
    std::unique_ptr<TestingPrefServiceSyncable> prefs =
        std::make_unique<TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    if (GetParam().feature_policy_value) {
      prefs->SetManagedPref(prefs::kBoundSessionCredentialsEnabled,
                            base::Value(*GetParam().feature_policy_value));
    }
    profile_builder.SetPrefService(std::move(prefs));

    original_profile_ = profile_builder.Build();
    if (otr_profile) {
      TestingProfile::Builder otr_builder;
      otr_builder.AddTestingFactory(
          UnexportableKeyServiceFactory::GetInstance(),
          base::BindRepeating(&CreateFakeUnexportableKeyService));
      otr_profile_ = otr_builder.BuildIncognito(original_profile_.get());
    }
  }

  bool ShouldServiceExistDiceEnabled() {
    return GetParam().expected_support ==
           switches::EnableBoundSessionCredentialsDiceSupport::kEnabled;
  }

  bool ShouldServiceExistAccountConsistencyDisabled() {
    return GetParam().expected_support.has_value();
  }

  TestingProfile* original_profile() { return original_profile_.get(); }
  TestingProfile* otr_profile() { return otr_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment;
  ScopedFeatureList feature_list;
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
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials, {{"dice-support", "enabled"}});

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
