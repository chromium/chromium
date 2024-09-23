// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::first_party_sets {

// This class is used to test how and when First-Party Sets policies are used
// during ash-ChromeOS startup. The expected behavior is that the policies
// aren't read from prefs until after user-login.
class FirstPartySetsPolicyInitializationTest : public LoginManagerTest {
 public:
  FirstPartySetsPolicyInitializationTest() {
    login_mixin_.AppendRegularUsers(1);
    test_account_id_ = login_mixin_.users()[0].account_id;
  }

 protected:
  // InProcessBrowserTest:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Allow policy fetches to fail so that these tests retrieve policy from a
    // MockConfigurationPolicyProvider.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  // If `overrides` is provided, this sets the FirstPartySetsEnabled and
  // FirstPartySetsOverrides policies with `true` and `overrides` respectively.
  //
  // If `overrides` is nullopt, this disables the FirstPartySetsEnabled policy.
  void SetFirstPartySetsPolicies(std::optional<base::Value::Dict> overrides) {
    policy_.Set(
        policy::key::kFirstPartySetsEnabled, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
        std::make_optional(base::Value(overrides.has_value())), nullptr);
    if (overrides.has_value()) {
      policy_.Set(policy::key::kFirstPartySetsOverrides,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                  std::make_optional(base::Value(std::move(overrides.value()))),
                  nullptr);
    }
    policy_provider_.UpdateChromePolicy(policy_);
  }

  AccountId test_account_id() { return test_account_id_; }

 private:
  LoginManagerMixin login_mixin_{&mixin_host_};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::PolicyMap policy_;
  AccountId test_account_id_;
};

// Verifies that policy defaults are used when not specified.
IN_PROC_BROWSER_TEST_F(FirstPartySetsPolicyInitializationTest, PolicyDefaults) {
  base::RunLoop loop;
  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GlobalTestingFactory
      factory = base::BindLambdaForTesting([&](content::BrowserContext*
                                                   context) {
        Profile* profile = Profile::FromBrowserContext(context);
        EXPECT_TRUE(profile->GetPrefs()
                        ->FindPreference(
                            prefs::kPrivacySandboxRelatedWebsiteSetsEnabled)
                        ->IsDefaultValue());
        EXPECT_TRUE(profile->GetPrefs()
                        ->FindPreference(
                            ::first_party_sets::kRelatedWebsiteSetsOverrides)
                        ->IsDefaultValue());
        loop.Quit();
        return base::WrapUnique<KeyedService>(
            new ::first_party_sets::FirstPartySetsPolicyService(context));
      });

  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance()
      ->SetTestingFactoryForTesting(std::move(factory));

  LoginUser(test_account_id());
  loop.Run();
}

// Set FirstPartySetsEnabled policy and check that it's ready in time for
// creation of the FirstPartySetsPolicyService for a user.
IN_PROC_BROWSER_TEST_F(FirstPartySetsPolicyInitializationTest,
                       EnabledPolicySetAndUsed) {
  base::RunLoop loop;
  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GlobalTestingFactory
      factory = base::BindLambdaForTesting([&](content::BrowserContext*
                                                   context) {
        Profile* profile = Profile::FromBrowserContext(context);
        // Only the FirstPartySetsEnabled pref was set.
        EXPECT_FALSE(profile->GetPrefs()
                         ->FindPreference(
                             prefs::kPrivacySandboxRelatedWebsiteSetsEnabled)
                         ->IsDefaultValue());
        EXPECT_TRUE(profile->GetPrefs()
                        ->FindPreference(
                            ::first_party_sets::kRelatedWebsiteSetsOverrides)
                        ->IsDefaultValue());
        // Check the expected value of FirstPartySetsEnabled the pref.
        EXPECT_EQ(profile->GetPrefs()->GetBoolean(
                      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled),
                  false);
        loop.Quit();
        return base::WrapUnique<KeyedService>(
            new ::first_party_sets::FirstPartySetsPolicyService(context));
      });

  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance()
      ->SetTestingFactoryForTesting(std::move(factory));

  SetFirstPartySetsPolicies(
      /*overrides=*/std::nullopt);
  LoginUser(test_account_id());
  loop.Run();
}

// Set both First-Party Sets policies and check that they are ready in time for
// creation of the FirstPartySetsPolicyService for a user.
IN_PROC_BROWSER_TEST_F(FirstPartySetsPolicyInitializationTest,
                       BothPoliciesSetAndUsed) {
  base::RunLoop loop;
  base::Value expected_overrides = base::JSONReader::Read(R"(
             {
                "replacements": [],
                "additions": []
              }
            )")
                                       .value();
  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GlobalTestingFactory
      factory = base::BindLambdaForTesting([&](content::BrowserContext*
                                                   context) {
        Profile* profile = Profile::FromBrowserContext(context);
        // Both prefs were set.
        EXPECT_FALSE(profile->GetPrefs()
                         ->FindPreference(
                             prefs::kPrivacySandboxRelatedWebsiteSetsEnabled)
                         ->IsDefaultValue());
        EXPECT_FALSE(profile->GetPrefs()
                         ->FindPreference(
                             ::first_party_sets::kRelatedWebsiteSetsOverrides)
                         ->IsDefaultValue());
        // Both prefs have the expected value.
        EXPECT_EQ(profile->GetPrefs()->GetBoolean(
                      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled),
                  true);
        EXPECT_TRUE(profile->GetPrefs()->GetDict(
                        ::first_party_sets::kRelatedWebsiteSetsOverrides) ==
                    expected_overrides.GetDict());
        loop.Quit();
        return base::WrapUnique<KeyedService>(
            new ::first_party_sets::FirstPartySetsPolicyService(context));
      });

  ::first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance()
      ->SetTestingFactoryForTesting(std::move(factory));

  SetFirstPartySetsPolicies(
      /*overrides=*/std::move(expected_overrides.Clone().GetDict()));
  LoginUser(test_account_id());
  loop.Run();
}

}  // namespace ash::first_party_sets
