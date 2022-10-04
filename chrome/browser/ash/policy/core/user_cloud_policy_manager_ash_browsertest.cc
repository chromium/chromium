// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace policy {

class UserCloudPolicyManagerTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<base::test::FeatureRef>,
                     ash::LoggedInUserMixin::LogInType>> {
 public:
  UserCloudPolicyManagerTest(const UserCloudPolicyManagerTest&) = delete;
  UserCloudPolicyManagerTest& operator=(const UserCloudPolicyManagerTest&) =
      delete;

 protected:
  UserCloudPolicyManagerTest() {
    // Override default tests configuration that prevents effective testing of
    // whether start-up URL policy is properly applied:
    // *   InProcessBrowserTest force about://blank start-up URL via command
    //     line (which trumps policy values).
    set_open_about_blank_on_browser_launch(false);

    scoped_feature_list_.InitWithFeatures(
        std::get<0>(GetParam()) /* enabled_features */,
        std::vector<base::test::FeatureRef>() /* disabled_features */);
  }

  ~UserCloudPolicyManagerTest() override = default;

  void TearDown() override {
    BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(nullptr);
    MixinBasedInProcessBrowserTest::TearDown();
  }

  // Sets up fake GAIA for specified user login, and requests login for the user
  // (using LoggedInUserMixin).
  void StartUserLogIn(bool wait_for_active_session) {
    logged_in_user_mixin_.LogInUser(true /*issue_any_scope_token*/,
                                    wait_for_active_session,
                                    /*request_policy_update=*/false);
  }

 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, std::get<1>(GetParam()) /*type*/, embedded_test_server(),
      this, true /*should_launch_browser*/,
      AccountId::FromUserEmailGaiaId(
          ash::FakeGaiaMixin::kEnterpriseUser1,
          ash::FakeGaiaMixin::kEnterpriseUser1GaiaId),
      // Initializing the login manager with no user will cause GAIA screen to
      // be shown on start-up.
      false /*include_initial_user*/};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, StartSession) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // User hasn't signed in yet, so shouldn't know if the user requires policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // Set up start-up URLs through a mandatory user policy.
  const char* const kStartupURLs[] = {"chrome://policy", "chrome://about"};
  enterprise_management::StringList* startup_urls_proto =
      logged_in_user_mixin_.GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_payload()
          ->mutable_restoreonstartupurls()
          ->mutable_value();
  for (auto* const url : kStartupURLs)
    startup_urls_proto->add_entries(url);
  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_restoreonstartup()
      ->set_value(SessionStartupPref::kPrefValueURLs);

  StartUserLogIn(true /*wait_for_active_session*/);

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  const int expected_tab_count = static_cast<int>(std::size(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }

  // User should be marked as requiring policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kPolicyRequired,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // It is expected that if ArcEnabled policy is not set then it is managed
  // by default and user is not able manually set it.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, ErrorLoadingPolicy) {
  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       ErrorLoadingPolicyForUnmanagedUser) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // User should be marked as not knowing if policy is required yet.
  // Mark user as not needing policy - errors loading policy should be
  // ignored (unlike previous ErrorLoadingPolicy test).
  known_user.SetProfileRequiresPolicy(
      logged_in_user_mixin_.GetAccountId(),
      user_manager::ProfileRequiresPolicy::kNoPolicyRequired);

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should still be marked as not needing policy
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kNoPolicyRequired,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       NoPolicyForNonEnterpriseUser) {
  // Recognize example.com as non-enterprise account. We don't use any
  // available public domain such as gmail.com in order to prevent possible
  // leak of verification keys/signatures.
  BrowserPolicyConnector::SetNonEnterpriseDomainForTesting("example.com");
  EXPECT_TRUE(BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // If a user signs in with a known non-enterprise account there should be no
  // policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as not requiring policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kNoPolicyRequired,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

using UserCloudPolicyManagerChildTest = UserCloudPolicyManagerTest;

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerChildTest, PolicyForChildUser) {
  BrowserPolicyConnector::SetNonEnterpriseDomainForTesting("example.com");
  EXPECT_TRUE(BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->Clear();
  StartUserLogIn(true /*wait_for_active_session*/);

  // User of CHILD type should be marked as requiring policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kPolicyRequired,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // It is expected that if ArcEnabled policy is not set then it is not managed
  // by default and user is able manually set it.
  EXPECT_FALSE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerChildTest,
                       PolicyForChildUserMissing) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  BrowserPolicyConnector::SetNonEnterpriseDomainForTesting("example.com");
  EXPECT_TRUE(BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));

  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            known_user.GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

const std::vector<base::test::FeatureRef> feature_lists[] = {
    {},
    {features::kDMServerOAuthForChildUser}};

// TODO(agawronska): Remove test instantiation with kDMServerOAuthForChildUser
// once it is enabled by default.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerTest,
    testing::Combine(
        testing::ValuesIn(feature_lists),
        testing::Values(ash::LoggedInUserMixin::LogInType::kRegular)));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerChildTest,
    testing::Combine(
        testing::ValuesIn(feature_lists),
        testing::Values(ash::LoggedInUserMixin::LogInType::kChild)));

}  // namespace policy
