// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
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
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, PRE_AllowedLanguages) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();
  PrefService* prefs = profile->GetPrefs();

  // Set locale and preferred languages to "en-US".
  prefs->SetString(language::prefs::kApplicationLocale, "en-US");
  prefs->SetString(language::prefs::kPreferredLanguages, "en-US");

  // Set policy to only allow "fr" as locale.
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>();
  base::ListValue allowed_languages;
  allowed_languages.AppendString("fr");
  policy->SetKey(key::kAllowedLanguages, std::move(allowed_languages));
  user_policy_helper()->SetPolicyAndWait(*policy, base::DictionaryValue(),
                                         profile);
}

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, AllowedLanguages) {
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();
  const PrefService* prefs = profile->GetPrefs();

  // Verifies that the default locale has been overridden by policy
  // (see |GetMandatoryPoliciesValue|)
  Browser* browser = CreateBrowser(profile);
  EXPECT_EQ("fr", prefs->GetString(language::prefs::kApplicationLocale));
  ui_test_utils::NavigateToURL(browser, GURL(chrome::kChromeUINewTabURL));
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
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();

  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      imm->GetActiveIMEState();
  ASSERT_TRUE(ime_state.get());

  std::vector<std::string> input_methods;
  input_methods.emplace_back("xkb:us::eng");
  input_methods.emplace_back("xkb:fr::fra");
  input_methods.emplace_back("xkb:de::ger");
  EXPECT_TRUE(imm->MigrateInputMethods(&input_methods));

  // No restrictions and current input method should be "xkb:us::eng" (default).
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[0], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[1]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow "xkb:fr::fra", "xkb:de::ger" an an invalid value
  // as input method.
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>();
  base::ListValue allowed_input_methods;
  allowed_input_methods.AppendString("xkb:fr::fra");
  allowed_input_methods.AppendString("xkb:de::ger");
  allowed_input_methods.AppendString("invalid_value_will_be_ignored");
  policy->SetKey(key::kAllowedInputMethods, std::move(allowed_input_methods));
  user_policy_helper()->SetPolicyAndWait(*policy, base::DictionaryValue(),
                                         profile);

  // Only "xkb:fr::fra", "xkb:de::ger" should be allowed, current input method
  // should be "xkb:fr::fra", enabling "xkb:us::eng" should not be possible,
  // enabling "xkb:de::ger" should be possible.
  EXPECT_EQ(2U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(2U, ime_state->GetActiveInputMethods()->size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_FALSE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow an invalid value as input method.
  std::unique_ptr<base::DictionaryValue> policy_invalid =
      std::make_unique<base::DictionaryValue>();
  base::ListValue invalid_input_methods;
  invalid_input_methods.AppendString("invalid_value_will_be_ignored");
  policy_invalid->SetKey(key::kAllowedInputMethods,
                         std::move(invalid_input_methods));
  user_policy_helper()->SetPolicyAndWait(*policy_invalid,
                                         base::DictionaryValue(), profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Allow all input methods again.
  user_policy_helper()->SetPolicyAndWait(base::DictionaryValue(),
                                         base::DictionaryValue(), profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));
}

class StartupBrowserWindowLaunchSuppressedTest : public LoginPolicyTestBase {
 public:
  StartupBrowserWindowLaunchSuppressedTest() = default;

  void SetUpPolicy(bool enabled) {
    std::unique_ptr<base::DictionaryValue> policy =
        std::make_unique<base::DictionaryValue>();

    policy->SetKey(key::kStartupBrowserWindowLaunchSuppressed,
                   base::Value(enabled));

    user_policy_helper()->SetPolicy(*policy, base::DictionaryValue());
  }

  void CheckLaunchedBrowserCount(unsigned int count) {
    SkipToLoginScreen();
    LogIn(kAccountId, kAccountPassword, kEmptyServices);

    Profile* const profile = GetProfileForActiveUser();

    ASSERT_EQ(count, chrome::GetBrowserCount(profile));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserWindowLaunchSuppressedTest);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(PrimaryUserPoliciesProxiedTest);
};

IN_PROC_BROWSER_TEST_F(PrimaryUserPoliciesProxiedTest,
                       AvailableInLocalStateEarly) {
  PolicyService* const device_wide_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPolicyService();

  // Sanity check default state without a policy active.
  EXPECT_FALSE(device_wide_policy_service
                   ->GetPolicies(PolicyNamespace(
                       POLICY_DOMAIN_CHROME, std::string() /* component_id */))
                   .GetValue(key::kAudioOutputAllowed));
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          chromeos::prefs::kAudioOutputAllowed);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_TRUE(pref->GetValue()->GetBool());

  base::DictionaryValue policy;
  policy.SetKey(key::kAudioOutputAllowed, base::Value(false));
  user_policy_helper()->SetPolicy(policy, base::DictionaryValue());

  SkipToLoginScreen();

  ProfileWaiter profile_waiter;
  TriggerLogIn(kAccountId, kAccountPassword, kEmptyServices);
  profile_waiter.WaitForProfileAdded();

  const base::Value* policy_value =
      device_wide_policy_service
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                        std::string() /* component_id */))
          .GetValue(key::kAudioOutputAllowed);
  ASSERT_TRUE(policy_value);
  EXPECT_FALSE(policy_value->GetBool());

  EXPECT_TRUE(pref->IsManaged());
  EXPECT_FALSE(pref->GetValue()->GetBool());

  // Make sure that session startup finishes before letting chrome exit.
  // Rationale: We've seen CHECK-failures when exiting chrome right after
  // a new profile is created, see e.g. https://crbug.com/1002066.
  chromeos::test::WaitForPrimaryUserSessionStart();
}

}  // namespace policy
