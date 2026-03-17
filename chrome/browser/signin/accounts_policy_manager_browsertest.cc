// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kManagedEmail[] = "user@managed.com";
constexpr char kNonManagedEmail[] = "user@gmail.com";

class AccountsPolicyManagerBrowserTest : public SigninBrowserTestBase {
 public:
  AccountsPolicyManagerBrowserTest() = default;
  ~AccountsPolicyManagerBrowserTest() override = default;

  AccountsPolicyManagerBrowserTest(const AccountsPolicyManagerBrowserTest&) =
      delete;
  AccountsPolicyManagerBrowserTest& operator=(
      const AccountsPolicyManagerBrowserTest&) = delete;

 private:
  base::AutoReset<bool> skip_message_box_for_test_{
      &chrome::internal::g_should_skip_message_box_for_test, true};
};

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       PRE_ProfileSigninAllowedPrefChanged_NonManaged) {
  // Sign in with a non-managed account.
  identity_test_env()->MakePrimaryAccountAvailable(
      kNonManagedEmail, signin::ConsentLevel::kSignin);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Toggle the pref.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup,
                                       false);

  // Verify that the user is still signed in (pref doesn't take effect
  // immediately).
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       ProfileSigninAllowedPrefChanged_NonManaged) {
  EXPECT_FALSE(GetProfile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  // Confirm that the user is no longer signed in to Chrome.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       PRE_ProfileSigninAllowedPrefChanged_Managed) {
  // Sign in with a managed account.
  identity_test_env()->MakePrimaryAccountAvailable(
      kManagedEmail, signin::ConsentLevel::kSignin);
  enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Toggle the pref.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup,
                                       false);

  // Verify that the user is still signed in (pref doesn't take effect
  // immediately).
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Also verify that the test has been using the initial profile.
  EXPECT_EQ(GetProfile()->GetPath(),
            g_browser_process->profile_manager()->user_data_dir().AppendASCII(
                chrome::kInitialProfile));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       ProfileSigninAllowedPrefChanged_Managed) {
  // `AccountsPolicyManager` should have deleted the original profile before the
  // test started.
  base::FilePath default_profile_path =
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(
          chrome::kInitialProfile);

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(default_profile_path);
  EXPECT_FALSE(entry);

  // The current profile should be different.
  EXPECT_NE(browser()->profile()->GetPath(), default_profile_path);
}

}  // namespace
