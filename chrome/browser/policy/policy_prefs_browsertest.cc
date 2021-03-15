// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_pref_mapping_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {

base::FilePath GetTestCasePath() {
  return ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("policy")),
      base::FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
}

}  // namespace

typedef InProcessBrowserTest PolicyPrefsTestCoverageTest;

IN_PROC_BROWSER_TEST_F(PolicyPrefsTestCoverageTest, AllPoliciesHaveATestCase) {
  VerifyAllPoliciesHaveATestCase(GetTestCasePath());
}

// Base class for tests that change policy.
class PolicyPrefsTest : public InProcessBrowserTest {
 public:
  PolicyPrefsTest() = default;
  PolicyPrefsTest(const PolicyPrefsTest&) = delete;
  PolicyPrefsTest& operator=(const PolicyPrefsTest&) = delete;
  ~PolicyPrefsTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void TearDownOnMainThread() override { ClearProviderPolicy(); }

  void ClearProviderPolicy() {
    provider_.UpdateChromePolicy(PolicyMap());
    base::RunLoop().RunUntilIdle();
  }

  MockConfigurationPolicyProvider provider_;
};

// Verifies that policies make their corresponding preferences become managed,
// and that the user can't override that setting.
IN_PROC_BROWSER_TEST_F(PolicyPrefsTest, PolicyToPrefsMapping) {
  PrefService* local_state = g_browser_process->local_state();
  PrefService* user_prefs = browser()->profile()->GetPrefs();

  VerifyPolicyToPrefMappings(GetTestCasePath(), local_state, user_prefs,
                             /* signin_profile_prefs= */ nullptr, &provider_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Class used to check policy to pref mappings for policies that are mapped into
// the sign-in profile (usually via LoginProfilePolicyProvider).
class SigninPolicyPrefsTest : public PolicyPrefsTest {
 public:
  SigninPolicyPrefsTest() = default;
  SigninPolicyPrefsTest(const SigninPolicyPrefsTest&) = delete;
  SigninPolicyPrefsTest& operator=(const SigninPolicyPrefsTest&) = delete;
  ~SigninPolicyPrefsTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyPrefsTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }
};

IN_PROC_BROWSER_TEST_F(SigninPolicyPrefsTest, PolicyToPrefsMapping) {
  PrefService* signin_profile_prefs =
      chromeos::ProfileHelper::GetSigninProfile()->GetPrefs();

  // Only checking signin_profile_prefs here since |local_state| is already
  // checked by PolicyPrefsTest.PolicyToPrefsMapping test.
  VerifyPolicyToPrefMappings(GetTestCasePath(), /* local_state= */ nullptr,
                             /* user_prefs= */ nullptr, signin_profile_prefs,
                             &provider_);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// For WebUI integration tests, see cr_policy_indicator_tests.js and
// cr_policy_pref_indicator_tests.js.

}  // namespace policy
