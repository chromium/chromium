// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, PRE_AllowedLanguages) {
  SkipToLoginScreen();
  LogIn();

  Profile* const profile = GetProfileForActiveUser();
  PrefService* prefs = profile->GetPrefs();

  // Set locale and preferred languages to "en-US".
  prefs->SetString(language::prefs::kApplicationLocale, "en-US");
  prefs->SetString(language::prefs::kPreferredLanguages, "en-US");

  // Set policy to only allow "fr" as locale.
  enterprise_management::CloudPolicySettings policy;
  policy.mutable_allowedlanguages()->mutable_value()->add_entries("fr");
  user_policy_helper()->SetPolicyAndWait(policy, profile);
}

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, AllowedLanguages) {
  LogIn();

  Profile* const profile = GetProfileForActiveUser();
  const PrefService* prefs = profile->GetPrefs();

  // Verifies that the default locale has been overridden by policy
  // (see |GetMandatoryPoliciesValue|)
  Browser* browser = CreateBrowser(profile);
  EXPECT_EQ("fr", prefs->GetString(language::prefs::kApplicationLocale));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser, GURL(chrome::kChromeUINewTabURL)));
  std::u16string french_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  std::u16string title;
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser, &title));
  EXPECT_EQ(french_title, title);

  // Make sure this is really French and differs from the English title.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  std::u16string english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);

  // Verifiy that the enforced locale is added into the list of
  // preferred languages.
  EXPECT_EQ("fr", prefs->GetString(language::prefs::kPreferredLanguages));
}

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, AllowedInputMethods) {
  SkipToLoginScreen();
  LogIn();

  Profile* const profile = GetProfileForActiveUser();

  auto* imm = ash::input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      imm->GetActiveIMEState();
  ASSERT_TRUE(ime_state.get());

  std::vector<std::string> input_methods;
  input_methods.emplace_back("xkb:us::eng");
  input_methods.emplace_back("xkb:fr::fra");
  input_methods.emplace_back("xkb:de::ger");
  EXPECT_TRUE(imm->GetMigratedInputMethodIDs(&input_methods));

  // No restrictions and current input method should be "xkb:us::eng" (default).
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethodIds().size());
  EXPECT_EQ(input_methods[0], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[1]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow "xkb:fr::fra", "xkb:de::ger" and an invalid value
  // as input method.
  enterprise_management::CloudPolicySettings policy;
  auto* allowed_input_methods =
      policy.mutable_allowedinputmethods()->mutable_value();
  allowed_input_methods->add_entries("xkb:fr::fra");
  allowed_input_methods->add_entries("xkb:de::ger");
  allowed_input_methods->add_entries("invalid_value_will_be_ignored");
  user_policy_helper()->SetPolicyAndWait(policy, profile);

  // Only "xkb:fr::fra", "xkb:de::ger" should be allowed, current input method
  // should be "xkb:fr::fra", enabling "xkb:us::eng" should not be possible,
  // enabling "xkb:de::ger" should be possible.
  EXPECT_EQ(2U, ime_state->GetAllowedInputMethodIds().size());
  EXPECT_EQ(2U, ime_state->GetEnabledInputMethods().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_FALSE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow an invalid value as input method.
  enterprise_management::CloudPolicySettings policy_invalid;
  policy_invalid.mutable_allowedinputmethods()->mutable_value()->add_entries(
      "invalid_value_will_be_ignored");
  user_policy_helper()->SetPolicyAndWait(policy_invalid, profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethodIds().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Allow all input methods again.
  user_policy_helper()->SetPolicyAndWait(
      enterprise_management::CloudPolicySettings(), profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethodIds().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));
}

class StartupBrowserWindowLaunchSuppressedTest : public LoginPolicyTestBase {
 public:
  StartupBrowserWindowLaunchSuppressedTest() = default;
  StartupBrowserWindowLaunchSuppressedTest(
      const StartupBrowserWindowLaunchSuppressedTest&) = delete;
  StartupBrowserWindowLaunchSuppressedTest& operator=(
      const StartupBrowserWindowLaunchSuppressedTest&) = delete;

  void SetUpPolicy(bool enabled) {
    enterprise_management::CloudPolicySettings policy;
    policy.mutable_startupbrowserwindowlaunchsuppressed()->set_value(enabled);
    user_policy_helper()->SetPolicy(policy);
  }

  void CheckLaunchedBrowserCount(unsigned int count) {
    SkipToLoginScreen();
    LogIn();

    Profile* const profile = GetProfileForActiveUser();

    ASSERT_EQ(count, chrome::GetBrowserCount(profile));
  }
};

// Test that the browser window is not launched when
// StartupBrowserWindowLaunchSuppressed is set to true.
IN_PROC_BROWSER_TEST_F(StartupBrowserWindowLaunchSuppressedTest,
                       TrueDoesNotAllowBrowserWindowLaunch) {
  SetUpPolicy(true);
  CheckLaunchedBrowserCount(0u);
}

// Test that the browser window is launched when
// StartupBrowserWindowLaunchSuppressed is set to false.
IN_PROC_BROWSER_TEST_F(StartupBrowserWindowLaunchSuppressedTest,
                       FalseAllowsBrowserWindowLaunch) {
  SetUpPolicy(false);
  CheckLaunchedBrowserCount(1u);
}

class PrimaryUserPoliciesProxiedTest : public LoginPolicyTestBase {
 public:
  PrimaryUserPoliciesProxiedTest() = default;
  PrimaryUserPoliciesProxiedTest(const PrimaryUserPoliciesProxiedTest&) =
      delete;
  PrimaryUserPoliciesProxiedTest& operator=(
      const PrimaryUserPoliciesProxiedTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(PrimaryUserPoliciesProxiedTest,
                       AvailableInLocalStateEarly) {
  PolicyService* const device_wide_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetPolicyService();

  // Sanity check default state without a policy active.
  EXPECT_FALSE(
      device_wide_policy_service
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                        std::string() /* component_id */))
          .GetValue(key::kAudioOutputAllowed, base::Value::Type::BOOLEAN));
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          ash::prefs::kAudioOutputAllowed);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_TRUE(pref->GetValue()->GetBool());

  enterprise_management::CloudPolicySettings policy;
  policy.mutable_audiooutputallowed()->set_value(false);
  user_policy_helper()->SetPolicy(policy);

  SkipToLoginScreen();

  ProfileWaiter profile_waiter;
  TriggerLogIn();
  profile_waiter.WaitForProfileAdded();

  const base::Value* policy_value =
      device_wide_policy_service
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                        std::string() /* component_id */))
          .GetValue(key::kAudioOutputAllowed, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(policy_value);
  EXPECT_FALSE(policy_value->GetBool());

  EXPECT_TRUE(pref->IsManaged());
  EXPECT_FALSE(pref->GetValue()->GetBool());

  // Make sure that session startup finishes before letting chrome exit.
  // Rationale: We've seen CHECK-failures when exiting chrome right after
  // a new profile is created, see e.g. https://crbug.com/1002066.
  ash::test::WaitForPrimaryUserSessionStart();
}

}  // namespace policy
