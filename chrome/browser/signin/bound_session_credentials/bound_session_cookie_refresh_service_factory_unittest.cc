// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
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
using base::test::FeatureRefAndParams;
using base::test::ScopedFeatureList;
using sync_preferences::TestingPrefServiceSyncable;
using ::testing::TestWithParam;
using ::testing::Values;

bool DoesServiceExistForProfile(Profile* profile) {
  return BoundSessionCookieRefreshServiceFactory::GetForProfile(profile) !=
         nullptr;
}

struct BoundSessionCookieRefreshServiceFactoryTestParams {
  std::string test_name;
  std::vector<FeatureRef> enabled_features;
  std::vector<FeatureRef> disabled_features;
  std::optional<bool> feature_policy_value;
  bool is_service_expected = false;
  ~BoundSessionCookieRefreshServiceFactoryTestParams() = default;
};

const BoundSessionCookieRefreshServiceFactoryTestParams kTestCases[] = {
    {
        "Enabled",
        {switches::kEnableBoundSessionCredentials},
        {},
        std::nullopt,
        true,
    },
    {
        "Default",
        {},
        {},
        std::nullopt,
        BUILDFLAG(IS_WIN),
    },
    {
        "Disabled",
        {},
        {switches::kEnableBoundSessionCredentials},
        std::nullopt,
        BUILDFLAG(IS_WIN),
    },
    {
        "DisabledByPolicy",
        {switches::kEnableBoundSessionCredentials},
        {},
        false,
        BUILDFLAG(IS_WIN),
    },
    {
        "EnabledByPolicy",
        {},
        {switches::kEnableBoundSessionCredentials},
        true,
        true,
    },
    {
        "DisabledWithContinuityEnabled",
        {kEnableBoundSessionCredentialsContinuity},
        {switches::kEnableBoundSessionCredentials},
        false,
        true,
    },
    {
        "DisabledWithExtrasDisabled",
        {},
        {switches::kEnableBoundSessionCredentials,
         kEnableBoundSessionCredentialsContinuity},
        std::nullopt,
        false,
    },
    {
        "EnabledWithExtrasDisabled",
        {switches::kEnableBoundSessionCredentials},
        {kEnableBoundSessionCredentialsContinuity},
        std::nullopt,
        true,
    },
    {
        "DisabledByKillSwitch",
        {switches::kBoundSessionCredentialsKillSwitch,
         switches::kEnableBoundSessionCredentials},
        {},
        true,
        false,
    },
};

}  // namespace

class BoundSessionCookieRefreshServiceFactoryTest
    : public TestWithParam<BoundSessionCookieRefreshServiceFactoryTestParams> {
 public:
  BoundSessionCookieRefreshServiceFactoryTest() {
    feature_list.InitWithFeatures(GetParam().enabled_features,
                                  GetParam().disabled_features);

    // `BoundSessionCookieRefreshService` depends on `UnexportableKeyService`,
    // ensure it is not null.
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::BindRepeating(
            [](crypto::UnexportableKeyProvider::Config config)
                -> std::unique_ptr<unexportable_keys::UnexportableKeyService> {
              return std::make_unique<
                  unexportable_keys::FakeUnexportableKeyService>();
            }));
  }

  void CreateProfile(bool otr_profile = false) {
    TestingProfile::Builder profile_builder = CreateProfileBuilder();
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
      otr_profile_ =
          CreateProfileBuilder().BuildIncognito(original_profile_.get());
    }
  }

  bool ShouldServiceExist() { return GetParam().is_service_expected; }

  TestingProfile* original_profile() { return original_profile_.get(); }
  TestingProfile* otr_profile() { return otr_profile_.get(); }

 private:
  TestingProfile::Builder CreateProfileBuilder() {
    TestingProfile::Builder builder;
    // Override `BoundSessionCookieRefreshServiceFactory` in order to bypass the
    // `ServiceIsNULLWhileTesting()` check.
    builder.AddTestingFactory(
        BoundSessionCookieRefreshServiceFactory::GetInstance(),
        base::BindRepeating(&BoundSessionCookieRefreshServiceFactoryTest::
                                CreateBoundSessionCookieRefreshService,
                            base::Unretained(this)));
    return builder;
  }

  std::unique_ptr<KeyedService> CreateBoundSessionCookieRefreshService(
      content::BrowserContext* context) {
    return BoundSessionCookieRefreshServiceFactory::GetInstance()
        ->BuildServiceInstanceForBrowserContext(context);
  }

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

  EXPECT_EQ(ShouldServiceExist(),
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
  EXPECT_EQ(ShouldServiceExist(),
            DoesServiceExistForProfile(original_profile()));
}

TEST_P(BoundSessionCookieRefreshServiceFactoryTest, OTRProfile) {
  CreateProfile(/*otr_profile=*/true);
  ASSERT_TRUE(otr_profile()->IsOffTheRecord());
  ASSERT_EQ(AccountConsistencyModeManager::GetMethodForProfile(otr_profile()),
            signin::AccountConsistencyMethod::kDisabled);
  EXPECT_EQ(ShouldServiceExist(), DoesServiceExistForProfile(otr_profile()));
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
  ASSERT_FALSE(UnexportableKeyServiceFactory::GetForProfileAndPurpose(
      profile.get(), UnexportableKeyServiceFactory::KeyPurpose::
                         kDeviceBoundSessionCredentialsPrototype));
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
