// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/policy/proto/policy_common_definitions.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace policy {

class UserCloudPolicyManagerTestBase : public MixinBasedInProcessBrowserTest {
 public:
  UserCloudPolicyManagerTestBase(const UserCloudPolicyManagerTestBase&) =
      delete;
  UserCloudPolicyManagerTestBase& operator=(
      const UserCloudPolicyManagerTestBase&) = delete;

 protected:
  UserCloudPolicyManagerTestBase(ash::LoggedInUserMixin::LogInType login_type,
                                 bool user_existed_before) {
    logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
        &mixin_host_, /*test_base=*/this, embedded_test_server(), login_type,
        user_existed_before);

    // Override default tests configuration that prevents effective testing of
    // whether start-up URL policy is properly applied:
    // *   InProcessBrowserTest force about://blank start-up URL via command
    //     line (which trumps policy values).
    set_open_about_blank_on_browser_launch(false);
  }

  ~UserCloudPolicyManagerTestBase() override = default;

  // Sets up fake GAIA for specified user login, and requests login for the user
  // (using LoggedInUserMixin).
  void StartUserLogIn(bool wait_for_active_session) {
    base::flat_set<ash::LoggedInUserMixin::LoginDetails> details{
        ash::LoggedInUserMixin::LoginDetails::kUseAnyScopeToken,
        ash::LoggedInUserMixin::LoginDetails::kNoPolicyForUser};

    if (!wait_for_active_session) {
      details.insert(ash::LoggedInUserMixin::LoginDetails::kDontWaitForSession);
    }
    logged_in_user_mixin_->LogInUser(details);
  }

  user_manager::ProfileRequiresPolicy GetProfileRequiresPolicy() {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    return known_user.GetProfileRequiresPolicy(
        logged_in_user_mixin_->GetAccountId());
  }

  void SetProfileRequiresPolicy(user_manager::ProfileRequiresPolicy value) {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetProfileRequiresPolicy(logged_in_user_mixin_->GetAccountId(),
                                        value);
  }

  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
};

// Test scenarios for enterprise-managed user signing in for the first time.
class UserCloudPolicyManagerNewManagedUserTest
    : public UserCloudPolicyManagerTestBase {
 protected:
  UserCloudPolicyManagerNewManagedUserTest()
      : UserCloudPolicyManagerTestBase(
            ash::LoggedInUserMixin::LogInType::kManaged,
            /*user_existed_before=*/false) {}
};

// Test scenarios for consumer user with custom domain signing in for the first
// time.
class UserCloudPolicyManagerNewConsumerUserTest
    : public UserCloudPolicyManagerTestBase {
 protected:
  UserCloudPolicyManagerNewConsumerUserTest()
      : UserCloudPolicyManagerTestBase(
            ash::LoggedInUserMixin::LogInType::kConsumerCustomDomain,
            /*user_existed_before=*/false) {}

  void SetUp() override {
    UserCloudPolicyManagerTestBase::SetUp();
    // Recognize example.com as non-enterprise account. We don't use any
    // available public domain such as gmail.com in order to prevent possible
    // leak of verification keys/signatures.
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        "example.com");
  }

  void TearDown() override {
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        nullptr);
    UserCloudPolicyManagerTestBase::TearDown();
  }
};

// Test scenarios for consumer user with custom domain who already signed in.
class UserCloudPolicyManagerExistingConsumerUserTest
    : public UserCloudPolicyManagerTestBase {
 protected:
  UserCloudPolicyManagerExistingConsumerUserTest()
      : UserCloudPolicyManagerTestBase(
            ash::LoggedInUserMixin::LogInType::kConsumerCustomDomain,
            /*user_existed_before=*/true) {}

  void SetUp() override {
    UserCloudPolicyManagerTestBase::SetUp();
    // Recognize example.com as non-enterprise account. We don't use any
    // available public domain such as gmail.com in order to prevent possible
    // leak of verification keys/signatures.
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        "example.com");
  }

  void TearDown() override {
    signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
        nullptr);
    UserCloudPolicyManagerTestBase::TearDown();
  }
};

struct FeaturesTestParam {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};

// Test scenarios for child user signing in for the first time.
class UserCloudPolicyManagerNewChildUserTest
    : public UserCloudPolicyManagerTestBase,
      public testing::WithParamInterface<FeaturesTestParam> {
 protected:
  UserCloudPolicyManagerNewChildUserTest()
      : UserCloudPolicyManagerTestBase(
            ash::LoggedInUserMixin::LogInType::kChild,
            /*user_existed_before=*/false) {
    scoped_feature_list_.InitWithFeatures(GetParam().enabled_features,
                                          GetParam().disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerNewChildUserTest,
    testing::Values(FeaturesTestParam{},
                    FeaturesTestParam{
                        .enabled_features = {
                            invalidation::kInvalidationsWithDirectMessages}}));

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerNewManagedUserTest, StartSession) {
  // User hasn't signed in yet, so shouldn't know if the user requires policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());

  // Set up start-up URLs through a mandatory user policy.
  const char* const kStartupURLs[] = {"chrome://policy", "chrome://about"};
  enterprise_management::StringList* startup_urls_proto =
      logged_in_user_mixin_->GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_payload()
          ->mutable_restoreonstartupurls()
          ->mutable_value();
  for (auto* const url : kStartupURLs)
    startup_urls_proto->add_entries(url);
  logged_in_user_mixin_->GetUserPolicyMixin()
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
            GetProfileRequiresPolicy());

  // It is expected that if ArcEnabled policy is not set then it is managed
  // by default and user is not able manually set it.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerNewManagedUserTest,
                       ErrorLoadingPolicy) {
  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());
}

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerExistingConsumerUserTest,
                       ErrorLoadingPolicyForUnmanagedUser) {
  // User should be marked as not knowing if policy is required yet.
  // Mark user as not needing policy - errors loading policy should be
  // ignored (unlike previous ErrorLoadingPolicy test).
  SetProfileRequiresPolicy(
      user_manager::ProfileRequiresPolicy::kNoPolicyRequired);

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should still be marked as not needing policy
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kNoPolicyRequired,
            GetProfileRequiresPolicy());
}

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerExistingConsumerUserTest,
                       NoPolicyForNonEnterpriseUser) {
  // Recognize example.com as non-enterprise account. We don't use any
  // available public domain such as gmail.com in order to prevent possible
  // leak of verification keys/signatures.
  signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
      "example.com");
  ASSERT_FALSE(
      signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          logged_in_user_mixin_->GetAccountId().GetUserEmail()));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // If a user signs in with a known non-enterprise account there should be no
  // policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as not requiring policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kNoPolicyRequired,
            GetProfileRequiresPolicy());
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerNewChildUserTest,
                       PolicyForChildUser) {
  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());

  logged_in_user_mixin_->GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->Clear();
  StartUserLogIn(true /*wait_for_active_session*/);

  // User of CHILD type should be marked as requiring policy.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kPolicyRequired,
            GetProfileRequiresPolicy());

  // It is expected that if ArcEnabled policy is not set then it is not managed
  // by default and user is able manually set it.
  EXPECT_FALSE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerNewChildUserTest,
                       PolicyForChildUserMissing) {
  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());

  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::ProfileRequiresPolicy::kUnknown,
            GetProfileRequiresPolicy());
}
}  // namespace policy
