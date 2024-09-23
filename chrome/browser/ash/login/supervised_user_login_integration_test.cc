// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/supervised_user_integration_base_test.h"
#include "chrome/test/base/chromeos/crosier/supervised_user_login_delegate.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "url/gurl.h"

// Tests using production GAIA can only run on branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

constexpr char kPolicyUrl[] = "chrome://policy";

// This class implements CrOS login using prod GAIA with the different types of
// supervised accounts.
class SupervisedUserLoginIntegrationTest
    : public SupervisedUserIntegrationBaseTest {
 public:
  auto OpenPolicyPage() {
    return Do([&]() { CreateBrowserWindow(GURL(kPolicyUrl)); });
  }

  // Checks that chrome://policy pages shows policies applied for the child
  // user.
  void VerifyPolicies() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPolicyTabId);
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPoliciesLoaded);
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kVerifyPolicies);
    const DeepQuery kPolicyTableQuery{"policy-table", ".main"};

    StateChange policies_loaded;
    policies_loaded.event = kPoliciesLoaded;
    policies_loaded.where = kPolicyTableQuery;
    policies_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    policies_loaded.test_function =
        "(el) => el.querySelectorAll('policy-row:not([hidden])').length > 0";

    StateChange verify_policies;
    verify_policies.event = kVerifyPolicies;
    verify_policies.where = kPolicyTableQuery;
    verify_policies.type = StateChange::Type::kExistsAndConditionTrue;
    verify_policies.test_function = R"(
      (el) => {
        const expectedPolicies = ['ArcPolicy', 'DeveloperToolsAvailability',
          'EduCoexistenceToSVersion', 'ForceGoogleSafeSearch',
          'LacrosSecondaryProfilesAllowed', 'ParentAccessCodeConfig',
          'PerAppTimeLimits', 'PerAppTimeLimitsAllowlist',
          'ReportArcStatusEnabled', 'URLBlocklist', 'UsageTimeLimit'];
        const policyNodes = el.querySelectorAll('policy-row:not([hidden])');
        const policies = [...policyNodes].map((node) =>
          node.shadowRoot.querySelector('.name').innerText)
        return expectedPolicies.every((policy) => policies.includes(policy));
      })";

    RunTestSequence(Log("Navigate to chrome://policy page"),
                    InstrumentNextTab(kPolicyTabId, AnyBrowser()),
                    OpenPolicyPage(),

                    Log("Wait for policies to load"),
                    WaitForWebContentsReady(kPolicyTabId, GURL(kPolicyUrl)),
                    WaitForStateChange(kPolicyTabId, policies_loaded),

                    Log("Check that all expected policies loaded"),
                    WaitForStateChange(kPolicyTabId, verify_policies));
  }
};

// Flaky: b/334993995
IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest,
                       DISABLED_TestUnicornLogin) {
  SetupContextWidget();

  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
  VerifyPolicies();
}

// Flaky: b/334993995
IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest,
                       DISABLED_TestGellerLogin) {
  SetupContextWidget();

  delegate_.set_user_type(
      SupervisedUserLoginDelegate::SupervisedUserType::kGeller);
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
  VerifyPolicies();
}

// Flaky: b/334993995
IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest,
                       DISABLED_TestGriffinLogin) {
  SetupContextWidget();

  delegate_.set_user_type(
      SupervisedUserLoginDelegate::SupervisedUserType::kGriffin);
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
  VerifyPolicies();
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
