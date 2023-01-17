// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kGuestUserDir[] = "Guest Profile";
const char kSignInUserDir[] = "Default";
const char kTestUserEmail[] = "test@example.com";
const char kTestUserEmailDir[] = "u-test@example.com-hash";

}  // namespace

class DiagnosticsBrowserDelegateImplTest : public testing::Test {
 public:
  DiagnosticsBrowserDelegateImplTest() : delegate_() {
    CHECK(profile_manager_.SetUp());
  }

  ~DiagnosticsBrowserDelegateImplTest() override = default;

  void SetUp() override {
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    LoginState::Initialize();

    task_env_.RunUntilIdle();
  }

  void TearDown() override {
    // Clean up user manager.
    user_manager_->Shutdown();
    user_manager_->Destroy();
    user_manager_.reset();

    LoginState::Shutdown();
    profile_manager_.DeleteAllTestingProfiles();

    // Let any pending tasks complete.
    task_env_.RunUntilIdle();
    testing::Test::TearDown();
  }

  // Get profile path based on user_data_dir path from current profile manager.
  base::FilePath GetExpectedPath(const std::string& path) {
    return profile_manager_.profile_manager()->user_data_dir().Append(path);
  }

  // Creates guest profile and user then sets that account to active.
  void LoginAsGuest() {
    auto* profile = profile_manager_.CreateGuestProfile();
    auto* user = user_manager_->AddGuestUser();
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    LoginAndSetActiveUserInUserManager(user_manager::GuestAccountId());
  }

  // Creates regular_user profile and user then sets that account to active.
  void LoginAsRegularTestUser() {
    const AccountId id = AccountId::FromUserEmail(kTestUserEmail);
    auto* user = user_manager_->AddUser(id);
    auto* profile = profile_manager_.CreateTestingProfile(kTestUserEmail);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    LoginAndSetActiveUserInUserManager(id);
  }

  // Looks up SignIn profile and creates regular user then sets that account to
  // active.
  void LoginAsSignInUser() {
    const AccountId id = user_manager::SignInAccountId();
    auto* profile = ProfileHelper::Get()->GetSigninProfile();
    auto* user = user_manager_->AddUser(id);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    LoginAndSetActiveUserInUserManager(id);
  }

  // Helper to set active user in the UserManager.
  void LoginAndSetActiveUserInUserManager(const AccountId& id) {
    user_manager_->LoginUser(id);
    user_manager_->SwitchActiveUser(id);
    task_env_.RunUntilIdle();
  }

 protected:
  diagnostics::DiagnosticsBrowserDelegateImpl delegate_;

 private:
  content::BrowserTaskEnvironment task_env_{};
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingPrefServiceSimple local_state_;
};

TEST_F(DiagnosticsBrowserDelegateImplTest, GetActiveUserProfileDirForSignIn) {
  const base::FilePath expected_path = GetExpectedPath(kSignInUserDir);

  LoginAsSignInUser();

  EXPECT_EQ(expected_path, delegate_.GetActiveUserProfileDir());
}

TEST_F(DiagnosticsBrowserDelegateImplTest,
       GetActiveUserProfileDirForOtherUsers) {
  const base::FilePath expected_path = GetExpectedPath(kGuestUserDir);

  LoginAsGuest();

  EXPECT_EQ(expected_path, delegate_.GetActiveUserProfileDir());
}

TEST_F(DiagnosticsBrowserDelegateImplTest,
       GetActiveUserProfileDirForRegularUser) {
  const base::FilePath expected_path = GetExpectedPath(kTestUserEmailDir);

  LoginAsRegularTestUser();

  EXPECT_EQ(expected_path, delegate_.GetActiveUserProfileDir());
}

TEST_F(DiagnosticsBrowserDelegateImplTest,
       GetActiveUserProfileDirForNoUserLoggedIn) {
  EXPECT_TRUE(user_manager::UserManager::IsInitialized());
  EXPECT_FALSE(user_manager::UserManager::Get()->IsUserLoggedIn());
  EXPECT_EQ(base::FilePath(), delegate_.GetActiveUserProfileDir());
}

}  // namespace diagnostics
}  // namespace ash
