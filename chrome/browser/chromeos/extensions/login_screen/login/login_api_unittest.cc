// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/ui/mock_login_display_host.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

const char kEmail[] = "email@test";
const char kGaiaId[] = "gaia@test";
const char kExtensionName[] = "extension_name";
const char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";

class MockExistingUserController : public chromeos::ExistingUserController {
 public:
  MockExistingUserController() = default;

  MockExistingUserController(const MockExistingUserController&) = delete;

  MockExistingUserController& operator=(const MockExistingUserController&) =
      delete;

  ~MockExistingUserController() override = default;

  MOCK_METHOD2(Login,
               void(const chromeos::UserContext&,
                    const chromeos::SigninSpecifics&));
  MOCK_CONST_METHOD0(IsSigninInProgress, bool());
};

class MockLoginApiLockHandler : public chromeos::LoginApiLockHandler {
 public:
  MockLoginApiLockHandler() {
    chromeos::LoginApiLockHandler::SetInstanceForTesting(this);
  }

  MockLoginApiLockHandler(const MockLoginApiLockHandler&) = delete;

  MockLoginApiLockHandler& operator=(const MockLoginApiLockHandler&) = delete;

  ~MockLoginApiLockHandler() override {
    chromeos::LoginApiLockHandler::SetInstanceForTesting(nullptr);
  }

  MOCK_METHOD0(RequestLockScreen, void());
  MOCK_METHOD2(Authenticate,
               void(const chromeos::UserContext& user_context,
                    base::OnceCallback<void(bool auth_success)> callback));
  MOCK_CONST_METHOD0(IsUnlockInProgress, bool());
};

// Wrapper which calls |DeleteTestingProfile()| on |profile| upon destruction.
class ScopedTestingProfile {
 public:
  ScopedTestingProfile(TestingProfile* profile,
                       TestingProfileManager* profile_manager)
      : profile_(profile), profile_manager_(profile_manager) {}

  ScopedTestingProfile(const ScopedTestingProfile&) = delete;

  ScopedTestingProfile& operator=(const ScopedTestingProfile&) = delete;

  ~ScopedTestingProfile() {
    profile_manager_->DeleteTestingProfile(profile_->GetProfileUserName());
  }

  TestingProfile* profile() { return profile_; }

 private:
  TestingProfile* const profile_;
  TestingProfileManager* const profile_manager_;
};

chromeos::UserContext GetPublicUserContext(const std::string& email) {
  return chromeos::UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                               AccountId::FromUserEmail(email));
}

void SetLoginExtensionApiLaunchExtensionIdPref(
    Profile* profile,
    const std::string& extension_id) {
  profile->GetPrefs()->SetString(prefs::kLoginExtensionApiLaunchExtensionId,
                                 extension_id);
}

}  // namespace

namespace extensions {

class LoginApiUnittest : public ExtensionApiUnittest {
 public:
  LoginApiUnittest() = default;

  LoginApiUnittest(const LoginApiUnittest&) = delete;

  LoginApiUnittest& operator=(const LoginApiUnittest&) = delete;

  ~LoginApiUnittest() override = default;

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    fake_chrome_user_manager_ = new ash::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::unique_ptr<ash::FakeChromeUserManager>(fake_chrome_user_manager_));
    mock_login_display_host_ =
        std::make_unique<chromeos::MockLoginDisplayHost>();
    mock_existing_user_controller_ =
        std::make_unique<MockExistingUserController>();
    // Set |LOGIN_PRIMARY| as the default state.
    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    EXPECT_CALL(*mock_login_display_host_, GetExistingUserController())
        .WillRepeatedly(Return(mock_existing_user_controller_.get()));

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    mock_existing_user_controller_.reset();
    mock_login_display_host_.reset();
    scoped_user_manager_.reset();

    ExtensionApiUnittest::TearDown();
  }

  void SetExtensionWithId(const std::string& extension_id) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(kExtensionName)
            .SetID(extension_id)
            .Build();
    set_extension(extension);
  }

  std::unique_ptr<ScopedTestingProfile> AddPublicAccountUser(
      const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    user_manager::User* user =
        fake_chrome_user_manager_->AddPublicAccountUser(account_id);
    TestingProfile* profile = profile_manager()->CreateTestingProfile(email);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile);

    return std::make_unique<ScopedTestingProfile>(profile, profile_manager());
  }

  ash::FakeChromeUserManager* fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<chromeos::MockLoginDisplayHost> mock_login_display_host_;
  std::unique_ptr<MockExistingUserController> mock_existing_user_controller_;
  // Sets up the global |SessionManager| instance.
  session_manager::SessionManager session_manager_;
};

MATCHER_P(MatchSigninSpecifics, expected, "") {
  return expected.guest_mode_url == arg.guest_mode_url &&
         expected.guest_mode_url_append_locale ==
             arg.guest_mode_url_append_locale &&
         expected.is_auto_login == arg.is_auto_login;
}

MATCHER_P(MatchUserContextSecret, expected, "") {
  return expected == arg.GetKey()->GetSecret();
}

// Test that calling |login.launchManagedGuestSession()| calls the corresponding
// method from the |ExistingUserController|.
TEST_F(LoginApiUnittest, LaunchManagedGuestSession) {
  base::TimeTicks now_ = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now_);

  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  EXPECT_CALL(*mock_existing_user_controller_,
              Login(GetPublicUserContext(kEmail),
                    MatchSigninSpecifics(chromeos::SigninSpecifics())))
      .Times(1);

  RunFunction(new LoginLaunchManagedGuestSessionFunction(), "[]");

  // Test that calling |login.launchManagedGuestSession()| triggered a user
  // activity in the |UserActivityDetector|.
  EXPECT_EQ(now_, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling |login.launchManagedGuestSession()| with a password sets
// the correct extension ID in the |UserContext| passed to
// |ExistingUserController|.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionWithPassword) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  chromeos::UserContext user_context = GetPublicUserContext(kEmail);
  user_context.SetKey(chromeos::Key("password"));
  EXPECT_CALL(
      *mock_existing_user_controller_,
      Login(user_context, MatchSigninSpecifics(chromeos::SigninSpecifics())))
      .Times(1);

  RunFunction(new LoginLaunchManagedGuestSessionFunction(), "[\"password\"]");
}

// Test that calling |login.launchManagedGuestSession()| returns an error when
// there are no managed guest session accounts.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionNoAccounts) {
  ASSERT_EQ(login_api_errors::kNoManagedGuestSessionAccounts,
            RunFunctionAndReturnError(
                new LoginLaunchManagedGuestSessionFunction(), "[]"));
}

// Test that calling |login.launchManagedGuestSession()| returns an error when
// the session state is not |LOGIN_PRIMARY|.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionWrongSessionState) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ASSERT_EQ(login_api_errors::kAlreadyActiveSession,
            RunFunctionAndReturnError(
                new LoginLaunchManagedGuestSessionFunction(), "[]"));
}

// Test that calling |login.launchManagedGuestSession()| returns an error when
// there is another signin in progress.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionSigninInProgress) {
  EXPECT_CALL(*mock_existing_user_controller_, IsSigninInProgress())
      .WillOnce(Return(true));
  ASSERT_EQ(login_api_errors::kAnotherLoginAttemptInProgress,
            RunFunctionAndReturnError(
                new LoginLaunchManagedGuestSessionFunction(), "[]"));
}

// Test that calling |login.exitCurrentSession()| with data for the next login
// attempt sets the |kLoginExtensionApiDataForNextLoginAttempt| pref to the
// given data.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithData) {
  const std::string data_for_next_login_attempt = "hello world";

  RunFunction(
      new LoginExitCurrentSessionFunction(),
      base::StringPrintf(R"(["%s"])", data_for_next_login_attempt.c_str()));

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_EQ(
      data_for_next_login_attempt,
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling |login.exitCurrentSession()| with no data clears the
// |kLoginExtensionApiDataForNextLoginAttempt| pref.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithNoData) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         "hello world");

  RunFunction(new LoginExitCurrentSessionFunction(), "[]");

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling |login.fetchDataForNextLoginAttempt()| function returns the
// value stored in the |kLoginExtensionsApiDataForNextLoginAttempt| pref and
// clears the pref.
TEST_F(LoginApiUnittest, FetchDataForNextLoginAttemptClearsPref) {
  const std::string data_for_next_login_attempt = "hello world";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         data_for_next_login_attempt);

  std::unique_ptr<base::Value> value(RunFunctionAndReturnValue(
      new LoginFetchDataForNextLoginAttemptFunction(), "[]"));
  ASSERT_EQ(data_for_next_login_attempt, value->GetString());

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

TEST_F(LoginApiUnittest, LockManagedGuestSession) {
  base::TimeTicks now_ = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now_);

  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  fake_chrome_user_manager_->set_current_user_can_lock(true);
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  MockLoginApiLockHandler mock_login_api_lock_handler;
  EXPECT_CALL(mock_login_api_lock_handler, RequestLockScreen())
      .WillOnce(Return());

  RunFunction(new LoginLockManagedGuestSessionFunction(), "[]");

  // Test that calling |login.lockManagedGuestSession()| triggered a user
  // activity in the |UserActivityDetector|.
  EXPECT_EQ(now_, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionNoActiveUser) {
  ASSERT_EQ(login_api_errors::kNoPermissionToLock,
            RunFunctionAndReturnError(
                new LoginLockManagedGuestSessionFunction(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionNotManagedGuestSession) {
  AccountId account_id = AccountId::FromGaiaId(kGaiaId);
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->SwitchActiveUser(account_id);

  ASSERT_EQ(login_api_errors::kNoPermissionToLock,
            RunFunctionAndReturnError(
                new LoginLockManagedGuestSessionFunction(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionUserCannotLock) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  fake_chrome_user_manager_->set_current_user_can_lock(false);

  ASSERT_EQ(login_api_errors::kNoPermissionToLock,
            RunFunctionAndReturnError(
                new LoginLockManagedGuestSessionFunction(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionSessionNotActive) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  fake_chrome_user_manager_->set_current_user_can_lock(true);
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(login_api_errors::kSessionIsNotActive,
            RunFunctionAndReturnError(
                new LoginLockManagedGuestSessionFunction(), "[]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSession) {
  base::TimeTicks now_ = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now_);

  SetExtensionWithId(kExtensionId);
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  SetLoginExtensionApiLaunchExtensionIdPref(scoped_profile->profile(),
                                            kExtensionId);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  MockLoginApiLockHandler mock_login_api_lock_handler;
  EXPECT_CALL(mock_login_api_lock_handler, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_login_api_lock_handler,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](chromeos::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/true);
      });

  RunFunction(new LoginUnlockManagedGuestSessionFunction(), "[\"password\"]");

  // Test that calling |login.unlockManagedGuestSession()| triggered a user
  // activity in the |UserActivityDetector|.
  EXPECT_EQ(now_, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionNoActiveUser) {
  ASSERT_EQ(
      login_api_errors::kNoPermissionToUnlock,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionNotManagedGuestSession) {
  AccountId account_id = AccountId::FromGaiaId(kGaiaId);
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->SwitchActiveUser(account_id);

  ASSERT_EQ(
      login_api_errors::kNoPermissionToUnlock,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionWrongExtensionId) {
  SetExtensionWithId(kExtensionId);
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  SetLoginExtensionApiLaunchExtensionIdPref(scoped_profile->profile(),
                                            "wrong_extension_id");
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  ASSERT_EQ(
      login_api_errors::kNoPermissionToUnlock,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionSessionNotLocked) {
  SetExtensionWithId(kExtensionId);
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  SetLoginExtensionApiLaunchExtensionIdPref(scoped_profile->profile(),
                                            kExtensionId);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  ASSERT_EQ(
      login_api_errors::kSessionIsNotLocked,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionUnlockInProgress) {
  SetExtensionWithId(kExtensionId);
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  SetLoginExtensionApiLaunchExtensionIdPref(scoped_profile->profile(),
                                            kExtensionId);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  MockLoginApiLockHandler mock_login_api_lock_handler;
  EXPECT_CALL(mock_login_api_lock_handler, IsUnlockInProgress())
      .WillOnce(Return(true));

  ASSERT_EQ(
      login_api_errors::kAnotherUnlockAttemptInProgress,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionAuthenticationFailed) {
  SetExtensionWithId(kExtensionId);
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  SetLoginExtensionApiLaunchExtensionIdPref(scoped_profile->profile(),
                                            kExtensionId);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  MockLoginApiLockHandler mock_login_api_lock_handler;
  EXPECT_CALL(mock_login_api_lock_handler, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_login_api_lock_handler,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](chromeos::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/false);
      });

  ASSERT_EQ(
      login_api_errors::kAuthenticationFailed,
      RunFunctionAndReturnError(new LoginUnlockManagedGuestSessionFunction(),
                                "[\"password\"]"));
}

}  // namespace extensions
