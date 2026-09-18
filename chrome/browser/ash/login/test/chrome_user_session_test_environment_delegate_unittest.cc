// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {
namespace {

class TestUserManagerObserver : public user_manager::UserManager::Observer {
 public:
  void Observe(user_manager::UserManager* user_manager) {
    observation_.Observe(user_manager);
  }

  void OnUserProfileCreated(const user_manager::User& user) override {
    created_users_.push_back(user.GetAccountId());
  }

  void OnUserProfileWillBeDestroyed(const user_manager::User& user) override {
    destroyed_users_.push_back(user.GetAccountId());
    observation_.Reset();
  }

  const std::vector<AccountId>& created_users() const { return created_users_; }
  const std::vector<AccountId>& destroyed_users() const {
    return destroyed_users_;
  }

 private:
  std::vector<AccountId> created_users_;
  std::vector<AccountId> destroyed_users_;
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
};

}  // namespace

class ChromeUserSessionTestEnvironmentDelegateTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
};

TEST_F(ChromeUserSessionTestEnvironmentDelegateTest,
       TestingProfileManagerReadyUponConstruction) {
  auto delegate = std::make_unique<ChromeUserSessionTestEnvironmentDelegate>(
      TestingBrowserProcess::GetGlobal());
  auto* delegate_ptr = delegate.get();

  UserSessionTestEnvironment env(
      TestingBrowserProcess::GetGlobal()->local_state(), std::move(delegate));

  // Immediately after UserSessionTestEnvironment construction completes,
  // TestingProfileManager must already be initialized and usable.
  EXPECT_TRUE(TestingBrowserProcess::GetGlobal()->profile_manager());
  EXPECT_EQ(delegate_ptr->testing_profile_manager().profile_manager(),
            TestingBrowserProcess::GetGlobal()->profile_manager());
}

TEST_F(ChromeUserSessionTestEnvironmentDelegateTest,
       LogInCreatesProfileAndConnectsUserManager) {
  const AccountId kAccountId =
      AccountId::FromUserEmailGaiaId("user@example.com", GaiaId("1234567890"));

  TestUserManagerObserver observer;
  {
    UserSessionTestEnvironment env(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));
    observer.Observe(user_manager::UserManager::Get());

    user_manager::User* user = env.AddRegularUser(kAccountId);
    ASSERT_TRUE(user);
    EXPECT_FALSE(user->is_profile_created());
    EXPECT_FALSE(user->GetProfilePrefs());
    EXPECT_TRUE(observer.created_users().empty());

    env.LogIn(kAccountId);

    // Verify profile is created with account_id.GetUserEmail() and annotated.
    Profile* profile = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kAccountId));
    ASSERT_TRUE(profile);
    EXPECT_EQ(profile->GetProfileUserName(), kAccountId.GetUserEmail());

    const AccountId* annotated_id = AnnotatedAccountId::Get(profile);
    ASSERT_TRUE(annotated_id);
    EXPECT_EQ(*annotated_id, kAccountId);

    // Verify ProfileUserManagerController automatically notified UserManager.
    EXPECT_TRUE(user->is_profile_created());
    EXPECT_EQ(user->GetProfilePrefs(), profile->GetPrefs());
    ASSERT_EQ(observer.created_users().size(), 1u);
    EXPECT_EQ(observer.created_users()[0], kAccountId);
    EXPECT_TRUE(observer.destroyed_users().empty());
  }

  // When UserSessionTestEnvironment is destroyed, profile destruction
  // automatically triggers OnUserProfileWillBeDestroyed on UserManager.
  ASSERT_EQ(observer.destroyed_users().size(), 1u);
  EXPECT_EQ(observer.destroyed_users()[0], kAccountId);
}

}  // namespace ash::test
