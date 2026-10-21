// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

namespace {

const char kGuestUserDir[] = "u-$guest-hash";
const char kTestUserEmail[] = "test@example.com";
const char kTestUserEmailDir[] = "u-test@example.com-hash";

}  // namespace

class DiagnosticsBrowserDelegateImplTest : public testing::Test {
 public:
  DiagnosticsBrowserDelegateImplTest() = default;
  ~DiagnosticsBrowserDelegateImplTest() override = default;

  void SetUp() override {
    LoginState::Initialize();
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override {
    user_session_test_environment_.reset();
    LoginState::Shutdown();
  }

  // Get profile path based on user_data_dir path from current profile manager.
  base::FilePath GetExpectedPath(const std::string& path) {
    return TestingBrowserProcess::GetGlobal()
        ->profile_manager()
        ->user_data_dir()
        .Append(path);
  }

  // Creates guest profile and user then sets that account to active.
  void LoginAsGuest() {
    const auto* user = user_session_test_environment_->AddGuestUser();
    user_session_test_environment_->LogIn(user->GetAccountId());
  }

  // Creates regular_user profile and user then sets that account to active.
  void LoginAsRegularTestUser() {
    const AccountId id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, GaiaId("12345"));
    const auto* user = user_session_test_environment_->AddRegularUser(id);
    user_session_test_environment_->LogIn(user->GetAccountId());
  }

 protected:
  diagnostics::DiagnosticsBrowserDelegateImpl delegate_;

 private:
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

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
