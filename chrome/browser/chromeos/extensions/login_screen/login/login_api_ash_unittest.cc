// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_ash.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/mock_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_done/external_logout_done_event_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/external_logout_request/external_logout_request_event_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
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
using testing::SaveArg;
using testing::StrictMock;

namespace {

constexpr char kEmail[] = "email@test";
constexpr char kGaiaId[] = "gaia";

const char kLaunchSamlUserSessionArguments[] =
    R"([{
          "email": "email@test",
          "gaiaId": "gaia",
          "password": "password",
          "oauthCode": "oauth_code"
       }])";

class MockExistingUserController : public ash::ExistingUserController {
 public:
  MockExistingUserController() = default;

  MockExistingUserController(const MockExistingUserController&) = delete;

  MockExistingUserController& operator=(const MockExistingUserController&) =
      delete;

  ~MockExistingUserController() override = default;

  MOCK_METHOD2(Login,
               void(const ash::UserContext&, const ash::SigninSpecifics&));
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
               void(const ash::UserContext& user_context,
                    base::OnceCallback<void(bool auth_success)> callback));
  MOCK_CONST_METHOD0(IsUnlockInProgress, bool());
};

// Wrapper which calls `DeleteTestingProfile()` on `profile` upon destruction.
class ScopedTestingProfile {
 public:
  ScopedTestingProfile(TestingProfile* profile,
                       TestingProfileManager* profile_manager,
                       const AccountId& account_id)
      : profile_(profile),
        profile_manager_(profile_manager),
        account_id_(account_id) {
    user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                           profile->GetPrefs());
  }

  ScopedTestingProfile(const ScopedTestingProfile&) = delete;

  ScopedTestingProfile& operator=(const ScopedTestingProfile&) = delete;

  ~ScopedTestingProfile() {
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(account_id_);
    std::string user_name = profile_->GetProfileUserName();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(user_name);
  }

  TestingProfile* profile() { return profile_; }

 private:
  raw_ptr<TestingProfile> profile_;
  const raw_ptr<TestingProfileManager> profile_manager_;
  const AccountId account_id_;
};

ash::UserContext GetPublicUserContext(const std::string& email) {
  return ash::UserContext(user_manager::UserType::kPublicAccount,
                          AccountId::FromUserEmail(email));
}

ash::UserContext GetRegularUserContext(const std::string& email,
                                       const std::string& gaia_id) {
  return ash::UserContext(user_manager::UserType::kRegular,
                          AccountId::FromUserEmailGaiaId(email, gaia_id));
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

    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
    fake_chrome_user_manager_ = new ash::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::unique_ptr<ash::FakeChromeUserManager>(fake_chrome_user_manager_));
    mock_login_display_host_ = std::make_unique<ash::MockLoginDisplayHost>();
    mock_existing_user_controller_ =
        std::make_unique<MockExistingUserController>();
    mock_lock_handler_ = std::make_unique<MockLoginApiLockHandler>();
    // Set `LOGIN_PRIMARY` as the default state.

    // SessionManager is created by
    // |AshTestHelper::bluetooth_config_test_helper()|.
    session_manager::SessionManager::Get()->SetSessionState(
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
    auth_events_recorder_.reset();

    ExtensionApiUnittest::TearDown();
  }

  std::unique_ptr<ScopedTestingProfile> AddPublicAccountUser(
      const std::string& email) {
    user_manager::User* user = fake_chrome_user_manager_->AddPublicAccountUser(
        AccountId::FromUserEmail(email));
    TestingProfile* profile = profile_manager()->CreateTestingProfile(email);

    return std::make_unique<ScopedTestingProfile>(profile, profile_manager(),
                                                  user->GetAccountId());
  }

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged>
      fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ash::MockLoginDisplayHost> mock_login_display_host_;
  std::unique_ptr<MockExistingUserController> mock_existing_user_controller_;
  std::unique_ptr<MockLoginApiLockHandler> mock_lock_handler_;
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
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

// Test that calling `login.launchManagedGuestSession()` calls the corresponding
// method from the `ExistingUserController`.
TEST_F(LoginApiUnittest, LaunchManagedGuestSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  EXPECT_CALL(*mock_existing_user_controller_,
              Login(GetPublicUserContext(kEmail),
                    MatchSigninSpecifics(ash::SigninSpecifics())))
      .Times(1);

  RunFunction(base::MakeRefCounted<LoginLaunchManagedGuestSessionFunction>(),
              "[]");

  // Test that calling `login.launchManagedGuestSession()` triggered a user
  // activity in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.launchManagedGuestSession()` with a password sets
// the correct password in the `UserContext` passed to
// `ExistingUserController`.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionWithPassword) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  ash::UserContext user_context = GetPublicUserContext(kEmail);
  user_context.SetKey(ash::Key("password"));
  EXPECT_CALL(*mock_existing_user_controller_,
              Login(user_context, MatchSigninSpecifics(ash::SigninSpecifics())))
      .Times(1);

  RunFunction(base::MakeRefCounted<LoginLaunchManagedGuestSessionFunction>(),
              "[\"password\"]");
}

// Test that calling `login.launchManagedGuestSession()` returns an error when
// there are no managed guest session accounts.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionNoAccounts) {
  ASSERT_EQ(login_api_errors::kNoManagedGuestSessionAccounts,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginLaunchManagedGuestSessionFunction>(),
                "[]"));
}

// Test that calling `login.launchManagedGuestSession()` returns an error when
// the session state is not `LOGIN_PRIMARY`.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionWrongSessionState) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ASSERT_EQ(login_api_errors::kAlreadyActiveSession,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginLaunchManagedGuestSessionFunction>(),
                "[]"));
}

// Test that calling `login.launchManagedGuestSession()` returns an error when
// there is another signin in progress.
TEST_F(LoginApiUnittest, LaunchManagedGuestSessionSigninInProgress) {
  EXPECT_CALL(*mock_existing_user_controller_, IsSigninInProgress())
      .WillOnce(Return(true));
  ASSERT_EQ(login_api_errors::kAnotherLoginAttemptInProgress,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginLaunchManagedGuestSessionFunction>(),
                "[]"));
}

// Test that calling `login.exitCurrentSession()` with data for the next login
// attempt sets the `kLoginExtensionApiDataForNextLoginAttempt` pref to the
// given data.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithData) {
  const std::string data_for_next_login_attempt = "hello world";

  RunFunction(
      base::MakeRefCounted<LoginExitCurrentSessionFunction>(),
      base::StringPrintf(R"(["%s"])", data_for_next_login_attempt.c_str()));

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_EQ(
      data_for_next_login_attempt,
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling `login.exitCurrentSession()` with no data clears the
// `kLoginExtensionApiDataForNextLoginAttempt` pref.
TEST_F(LoginApiUnittest, ExitCurrentSessionWithNoData) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         "hello world");

  RunFunction(base::MakeRefCounted<LoginExitCurrentSessionFunction>(), "[]");

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling `login.fetchDataForNextLoginAttempt()` returns the value
// stored in the `kLoginExtensionsApiDataForNextLoginAttempt` pref and
// clears the pref.
TEST_F(LoginApiUnittest, FetchDataForNextLoginAttemptClearsPref) {
  const std::string data_for_next_login_attempt = "hello world";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         data_for_next_login_attempt);

  std::optional<base::Value> value = RunFunctionAndReturnValue(
      base::MakeRefCounted<LoginFetchDataForNextLoginAttemptFunction>(), "[]");
  ASSERT_EQ(data_for_next_login_attempt, value->GetString());

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

// Test that calling `login.setDataForNextLoginAttempt()` sets the
// value stored in the `kLoginExtensionsApiDataForNextLoginAttempt` pref.
TEST_F(LoginApiUnittest, SetDataForNextLoginAttempt) {
  const std::string data_for_next_login_attempt = "hello world";

  std::optional<base::Value> value = RunFunctionAndReturnValue(
      base::MakeRefCounted<LoginSetDataForNextLoginAttemptFunction>(),
      "[\"" + data_for_next_login_attempt + "\"]");

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_EQ(
      data_for_next_login_attempt,
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

TEST_F(LoginApiUnittest, LockManagedGuestSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  RunFunction(base::MakeRefCounted<LoginLockManagedGuestSessionFunction>(),
              "[]");

  // Test that calling `login.lockManagedGuestSession()` triggered a user
  // activity in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest,
       LockManagedGuestSessionWithLockCurrentSessionFunction) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  auto function = base::MakeRefCounted<LoginLockCurrentSessionFunction>();
  RunFunction(function.get(), "[]");

  // Test that calling `login.lockCurrentSession()` triggered a user activity in
  // the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionNoActiveUser) {
  ASSERT_EQ(
      login_api_errors::kNoLockableSession,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLockManagedGuestSessionFunction>(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionNotManagedGuestSession) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->SwitchActiveUser(account_id);

  ASSERT_EQ(
      login_api_errors::kNoLockableSession,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLockManagedGuestSessionFunction>(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionUserCannotLock) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, false);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             false);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  ASSERT_EQ(
      login_api_errors::kNoLockableSession,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLockManagedGuestSessionFunction>(), "[]"));
}

TEST_F(LoginApiUnittest, LockManagedGuestSessionSessionNotActive) {
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(
      login_api_errors::kSessionIsNotActive,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLockManagedGuestSessionFunction>(), "[]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_lock_handler_,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](ash::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/true);
      });

  RunFunction(base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
              "[\"password\"]");

  // Test that calling `login.unlockManagedGuestSession()` triggered a user
  // activity in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest,
       UnlockManagedGuestSessionWithUnlockCurrentSessionFunction) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_lock_handler_,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](ash::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/true);
      });

  auto function = base::MakeRefCounted<LoginUnlockCurrentSessionFunction>();
  RunFunction(function.get(), "[\"password\"]");

  // Test that calling `login.unlockCurrentSession()` triggered a user activity
  // in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionNoActiveUser) {
  ASSERT_EQ(login_api_errors::kNoUnlockableSession,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionNotManagedGuestSession) {
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);
  fake_chrome_user_manager_->AddUser(account_id);
  fake_chrome_user_manager_->SwitchActiveUser(account_id);

  ASSERT_EQ(login_api_errors::kNoUnlockableSession,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionCannotUnlock) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  ASSERT_EQ(login_api_errors::kNoUnlockableSession,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionSessionNotLocked) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  ASSERT_EQ(login_api_errors::kSessionIsNotLocked,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionUnlockInProgress) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress()).WillOnce(Return(true));

  ASSERT_EQ(login_api_errors::kAnotherUnlockAttemptInProgress,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

TEST_F(LoginApiUnittest, UnlockManagedGuestSessionAuthenticationFailed) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_lock_handler_,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](ash::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/false);
      });

  ASSERT_EQ(login_api_errors::kAuthenticationFailed,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockManagedGuestSessionFunction>(),
                "[\"password\"]"));
}

class LoginApiUserSessionUnittest : public LoginApiUnittest {
 public:
  LoginApiUserSessionUnittest() = default;

  LoginApiUserSessionUnittest(const LoginApiUserSessionUnittest&) = delete;

  LoginApiUserSessionUnittest& operator=(const LoginApiUserSessionUnittest&) =
      delete;

  ~LoginApiUserSessionUnittest() override = default;

 protected:
  std::unique_ptr<ScopedTestingProfile> AddRegularUser(
      const std::string& email) {
    auto* user = fake_chrome_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmailGaiaId(email, kGaiaId),
        /* is_affiliated= */ true);
    TestingProfile* profile = profile_manager()->CreateTestingProfile(email);

    return std::make_unique<ScopedTestingProfile>(profile, profile_manager(),
                                                  user->GetAccountId());
  }
};

MATCHER_P(MatchUserContext, expected, "") {
  return expected.GetGaiaID() == arg.GetGaiaID() &&
         expected.GetKey()->GetSecret() == arg.GetKey()->GetSecret() &&
         expected.GetAuthCode() == arg.GetAuthCode();
}

// Test that calling `login.launchSamlUserSession()` calls the corresponding
// method from the `LoginDisplayHost` and sets the correct attributes in the
// `UserContext`
TEST_F(LoginApiUserSessionUnittest, LaunchSamlUserSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> profile = AddRegularUser(kEmail);
  ash::UserContext user_context = GetRegularUserContext(kEmail, kGaiaId);

  ash::Key key("password");
  key.SetLabel(ash::kCryptohomeGaiaKeyLabel);
  user_context.SetKey(key);
  user_context.SetPasswordKey(ash::Key("password"));
  user_context.SetAuthFlow(ash::UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  user_context.SetIsUsingSamlPrincipalsApi(false);
  user_context.SetAuthCode("oauth_code");

  EXPECT_CALL(*mock_login_display_host_,
              CompleteLogin(MatchUserContext(user_context)))
      .Times(1);

  auto function = base::MakeRefCounted<LoginLaunchSamlUserSessionFunction>();
  RunFunction(function.get(), kLaunchSamlUserSessionArguments);

  // Test that calling `login.launchSamlUserSession()` triggered a user activity
  // in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.launchSamlUserSession()` returns an error when the
// session state is not `LOGIN_PRIMARY`.
TEST_F(LoginApiUserSessionUnittest, LaunchSamlUserSessionWrongSessionState) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  auto function = base::MakeRefCounted<LoginLaunchSamlUserSessionFunction>();
  ASSERT_EQ(login_api_errors::kAlreadyActiveSession,
            RunFunctionAndReturnError(function.get(),
                                      kLaunchSamlUserSessionArguments));
}

// Test that calling `login.launchSamlUserSession()` returns an error when there
//  is another signin in progress.
TEST_F(LoginApiUserSessionUnittest, LaunchSamlUserSessionSigninInProgress) {
  EXPECT_CALL(*mock_existing_user_controller_, IsSigninInProgress())
      .WillOnce(Return(true));

  auto function = base::MakeRefCounted<LoginLaunchSamlUserSessionFunction>();
  ASSERT_EQ(login_api_errors::kAnotherLoginAttemptInProgress,
            RunFunctionAndReturnError(function.get(),
                                      kLaunchSamlUserSessionArguments));
}

// Test that calling `login.lockCurrentSession()` calls the corresponding method
// from the `LoginApiLockHandler` for regular user.
TEST_F(LoginApiUserSessionUnittest, LockUserSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> profile = AddRegularUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  auto function = base::MakeRefCounted<LoginLockCurrentSessionFunction>();
  RunFunction(function.get(), "[]");

  // Test that calling `login.lockCurrentSession()` triggered a user activity in
  // the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.lockCurrentSession()` returns an error when the user
// session is not active for regular user.
TEST_F(LoginApiUserSessionUnittest, LockUserSessionSessionNotActive) {
  std::unique_ptr<ScopedTestingProfile> profile = AddRegularUser(kEmail);
  profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  profile->profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                             true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto function = base::MakeRefCounted<LoginLockCurrentSessionFunction>();
  ASSERT_EQ(login_api_errors::kSessionIsNotActive,
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Test that calling `login.unlockCurrentSession()` calls the corresponding
// method from the `LoginApiLockHandler` for regular user.
TEST_F(LoginApiUserSessionUnittest, UnlockUserSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  std::unique_ptr<ScopedTestingProfile> scoped_profile = AddRegularUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_lock_handler_,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](ash::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/true);
      });

  auto function = base::MakeRefCounted<LoginUnlockCurrentSessionFunction>();
  RunFunction(function.get(), "[\"password\"]");

  // Test that calling `login.unlockCurrentSession()` triggered a user activity
  // in the `UserActivityDetector`.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.unlockCurrentSession()` returns an error when the
// user session is not locked for regular user.
TEST_F(LoginApiUserSessionUnittest, UnlockUserSessionSessionNotLocked) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile = AddRegularUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));

  auto function = base::MakeRefCounted<LoginUnlockCurrentSessionFunction>();
  ASSERT_EQ(login_api_errors::kSessionIsNotLocked,
            RunFunctionAndReturnError(function.get(), "[\"password\"]"));
}

// Test that calling `login.unlockCurrentSession()` returns an error when an
// unlock is already in progress for regular user.
TEST_F(LoginApiUserSessionUnittest, UnlockUserSessionUnlockInProgress) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile = AddRegularUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress()).WillOnce(Return(true));

  auto function = base::MakeRefCounted<LoginUnlockCurrentSessionFunction>();
  ASSERT_EQ(login_api_errors::kAnotherUnlockAttemptInProgress,
            RunFunctionAndReturnError(function.get(), "[\"password\"]"));
}

// Test that calling `login.unlockCurrentSession()` returns false via callback
// on failed authentication for regular user.
TEST_F(LoginApiUserSessionUnittest, UnlockUserSessionAuthenticationFailed) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile = AddRegularUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  fake_chrome_user_manager_->SwitchActiveUser(AccountId::FromUserEmail(kEmail));
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, IsUnlockInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_lock_handler_,
              Authenticate(MatchUserContextSecret("password"), _))
      .WillOnce([](ash::UserContext user_context,
                   base::OnceCallback<void(bool auth_success)> callback) {
        std::move(callback).Run(/*auth_success=*/false);
      });

  auto function = base::MakeRefCounted<LoginUnlockCurrentSessionFunction>();
  ASSERT_EQ(login_api_errors::kAuthenticationFailed,
            RunFunctionAndReturnError(function.get(), "[\"password\"]"));
}

class LoginApiSharedSessionUnittest : public LoginApiUnittest {
 public:
  LoginApiSharedSessionUnittest() = default;

  LoginApiSharedSessionUnittest(const LoginApiSharedSessionUnittest&) = delete;

  LoginApiSharedSessionUnittest& operator=(
      const LoginApiSharedSessionUnittest&) = delete;

  ~LoginApiSharedSessionUnittest() override = default;

 protected:
  void SetUp() override {
    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(
        ash::kDeviceRestrictedManagedGuestSessionEnabled, true);
    // Remove cleanup handlers.
    chromeos::CleanupManagerAsh::Get()->SetCleanupHandlersForTesting({});

    LoginApiUnittest::SetUp();
  }

  void TearDown() override {
    GetCrosSettingsHelper()->RestoreRealDeviceSettingsProvider();
    chromeos::SharedSessionHandler::Get()->ResetStateForTesting();
    chromeos::CleanupManagerAsh::Get()->ResetCleanupHandlersForTesting();
    testing_profile_.reset();

    LoginApiUnittest::TearDown();
  }

  void SetUpCleanupHandlerMocks(
      std::optional<std::string> error1 = std::nullopt,
      std::optional<std::string> error2 = std::nullopt) {
    std::unique_ptr<chromeos::MockCleanupHandler> mock_cleanup_handler1 =
        std::make_unique<StrictMock<chromeos::MockCleanupHandler>>();
    EXPECT_CALL(*mock_cleanup_handler1, Cleanup(_))
        .WillOnce(Invoke(
            ([error1](
                 chromeos::CleanupHandler::CleanupHandlerCallback callback) {
              std::move(callback).Run(error1);
            })));
    std::unique_ptr<chromeos::MockCleanupHandler> mock_cleanup_handler2 =
        std::make_unique<StrictMock<chromeos::MockCleanupHandler>>();
    EXPECT_CALL(*mock_cleanup_handler2, Cleanup(_))
        .WillOnce(Invoke(
            ([error2](
                 chromeos::CleanupHandler::CleanupHandlerCallback callback) {
              std::move(callback).Run(error2);
            })));

    std::map<std::string, std::unique_ptr<chromeos::CleanupHandler>>
        cleanup_handlers;
    cleanup_handlers.insert({"Handler1", std::move(mock_cleanup_handler1)});
    cleanup_handlers.insert({"Handler2", std::move(mock_cleanup_handler2)});
    chromeos::CleanupManagerAsh::Get()->SetCleanupHandlersForTesting(
        std::move(cleanup_handlers));
  }

  void SetUpCleanupHandlerMockNotCalled() {
    std::unique_ptr<chromeos::MockCleanupHandler> mock_cleanup_handler =
        std::make_unique<chromeos::MockCleanupHandler>();
    EXPECT_CALL(*mock_cleanup_handler, Cleanup(_)).Times(0);
    std::map<std::string, std::unique_ptr<chromeos::CleanupHandler>>
        cleanup_handlers;
    cleanup_handlers.insert({"Handler", std::move(mock_cleanup_handler)});
    chromeos::CleanupManagerAsh::Get()->SetCleanupHandlersForTesting(
        std::move(cleanup_handlers));
  }

  void LaunchSharedManagedGuestSession(const std::string& password) {
    EXPECT_CALL(*mock_existing_user_controller_,
                Login(_, MatchSigninSpecifics(ash::SigninSpecifics())))
        .Times(1);

    testing_profile_ = AddPublicAccountUser(kEmail);

    RunFunction(
        base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
        "[\"" + password + "\"]");

    testing_profile_->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
    testing_profile_->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kAllowScreenLock, true);

    fake_chrome_user_manager_->SwitchActiveUser(
        AccountId::FromUserEmail(kEmail));

    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void ExpectAuthenticateWithSessionSecret(bool auth_success) {
    const std::string& session_secret =
        chromeos::SharedSessionHandler::Get()->GetSessionSecretForTesting();
    EXPECT_CALL(*mock_lock_handler_,
                Authenticate(MatchUserContextSecret(session_secret), _))
        .WillOnce([auth_success](
                      ash::UserContext user_context,
                      base::OnceCallback<void(bool auth_success)> callback) {
          std::move(callback).Run(/*auth_success=*/auth_success);
        });
  }

  std::unique_ptr<ScopedTestingProfile> testing_profile_;
};

// Test that calling `login.launchSharedManagedGuestSession()` sets the correct
// extension ID and session secret in the `UserContext` passed to
// `ExistingUserController`, and sets user hash and salt.
TEST_F(LoginApiSharedSessionUnittest, LaunchSharedManagedGuestSession) {
  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);
  std::unique_ptr<ScopedTestingProfile> profile = AddPublicAccountUser(kEmail);
  ash::UserContext user_context;
  EXPECT_CALL(*mock_existing_user_controller_,
              Login(_, MatchSigninSpecifics(ash::SigninSpecifics())))
      .WillOnce(SaveArg<0>(&user_context));

  RunFunction(
      base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
      "[\"foo\"]");

  EXPECT_TRUE(user_context.CanLockManagedGuestSession());
  chromeos::SharedSessionHandler* handler =
      chromeos::SharedSessionHandler::Get();
  const std::string& session_secret = handler->GetSessionSecretForTesting();
  EXPECT_EQ(user_context.GetKey()->GetSecret(), session_secret);
  EXPECT_NE("", session_secret);
  EXPECT_NE("", handler->GetUserSecretHashForTesting());
  EXPECT_NE("", handler->GetUserSecretSaltForTesting());

  // Test that user activity is triggered.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.launchSharedManagedGuestSession()` returns an error
// when the DeviceRestrictedManagedGuestSessionEnabled policy is set to false.
TEST_F(LoginApiSharedSessionUnittest,
       LaunchSharedManagedGuestSessionRestrictedMGSNotEnabled) {
  GetCrosSettingsHelper()->SetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled, false);

  ASSERT_EQ(
      login_api_errors::kDeviceRestrictedManagedGuestSessionNotEnabled,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
          "[\"foo\"]"));
}

// Test that calling `login.launchSharedManagedGuestSession()` returns an error
// when there are no managed guest session accounts.
TEST_F(LoginApiSharedSessionUnittest,
       LaunchSharedManagedGuestSessionNoAccounts) {
  ASSERT_EQ(
      login_api_errors::kNoManagedGuestSessionAccounts,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
          "[\"foo\"]"));
}

// Test that calling `login.launchSharedManagedGuestSession()` returns an error
// when the session state is not `LOGIN_PRIMARY`.
TEST_F(LoginApiSharedSessionUnittest,
       LaunchSharedManagedGuestSessionWrongSessionState) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  ASSERT_EQ(
      login_api_errors::kLoginScreenIsNotActive,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
          "[\"foo\"]"));
}

// Test that calling `login.launchSharedManagedGuestSession()` returns an error
// when there is another signin in progress.
TEST_F(LoginApiSharedSessionUnittest,
       LaunchSharedManagedGuestSessionSigninInProgress) {
  EXPECT_CALL(*mock_existing_user_controller_, IsSigninInProgress())
      .WillOnce(Return(true));
  ASSERT_EQ(
      login_api_errors::kAnotherLoginAttemptInProgress,
      RunFunctionAndReturnError(
          base::MakeRefCounted<LoginLaunchSharedManagedGuestSessionFunction>(),
          "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` works.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSession) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/true);

  base::TimeTicks now = base::TimeTicks::Now();
  ui::UserActivityDetector::Get()->set_now_for_test(now);

  RunFunction(base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
              "[\"foo\"]");

  // Test that user activity is triggered.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.unlockSharedSession()` returns an error when the
// session is not locked.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionNotLocked) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  ASSERT_EQ(login_api_errors::kSessionIsNotLocked,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when there
// is no shared MGS launched.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionNoSharedMGS) {
  std::unique_ptr<ScopedTestingProfile> scoped_profile =
      AddPublicAccountUser(kEmail);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, true);
  scoped_profile->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, true);

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  ASSERT_EQ(login_api_errors::kNoSharedMGSFound,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when there
// is no shared session active.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionNoSharedSession) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");
  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());
  RunFunction(base::MakeRefCounted<LoginEndSharedSessionFunction>(), "[]");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(login_api_errors::kSharedSessionIsNotActive,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when a
// different password is used.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionAuthenticationFailed) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(login_api_errors::kAuthenticationFailed,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"bar\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when there
// is an error when unlocking the screen.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionUnlockFailed) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/false);

  ASSERT_EQ(login_api_errors::kUnlockFailure,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when the
// MGS cannot be unlocked.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionCannotUnlock) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  testing_profile_->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kLoginExtensionApiCanLockManagedGuestSession, false);
  testing_profile_->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kAllowScreenLock, false);

  ASSERT_EQ(login_api_errors::kNoUnlockableSession,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.unlockSharedSession()` returns an error when there
// is a cleanup in progress.
TEST_F(LoginApiSharedSessionUnittest, UnlockSharedSessionCleanupInProgress) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  chromeos::CleanupManagerAsh::Get()->SetIsCleanupInProgressForTesting(true);

  ASSERT_EQ(login_api_errors::kCleanupInProgress,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"foo\"]"));
}

// Test that calling `login.endSharedSession()` clears the user hash and salt
// and locks the screen when the screen is not locked.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSession) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  RunFunction(new LoginEndSharedSessionFunction(), "[]");

  chromeos::SharedSessionHandler* handler =
      chromeos::SharedSessionHandler::Get();
  EXPECT_EQ("", handler->GetUserSecretHashForTesting());
  EXPECT_EQ("", handler->GetUserSecretSaltForTesting());
}

// Test that calling `login.endSharedSession()` works on the lock screen as
// well.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSessionLocked) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).Times(0);

  RunFunction(new LoginEndSharedSessionFunction(), "[]");

  chromeos::SharedSessionHandler* handler =
      chromeos::SharedSessionHandler::Get();
  EXPECT_EQ("", handler->GetUserSecretHashForTesting());
  EXPECT_EQ("", handler->GetUserSecretSaltForTesting());
}

// Test that calling `login.endSharedSession()` returns an error when the there
// is an error in the cleanup handlers.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSessionCleanupError) {
  std::string error1 = "Mock cleanup handler 1 error";
  std::string error2 = "Mock cleanup handler 2 error";
  SetUpCleanupHandlerMocks(error1, error2);
  LaunchSharedManagedGuestSession("foo");

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  ASSERT_EQ(
      "Handler1: " + error1 + "\nHandler2: " + error2,
      RunFunctionAndReturnError(new LoginEndSharedSessionFunction(), "[]"));
}

// Test that calling `login.endSharedSession()` returns an error when no shared
// MGS was launched.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSessionNoSharedMGS) {
  SetUpCleanupHandlerMockNotCalled();
  ASSERT_EQ(
      login_api_errors::kNoSharedMGSFound,
      RunFunctionAndReturnError(new LoginEndSharedSessionFunction(), "[]"));
}

// Test that calling `login.endSharedSession()` returns an error when there is
// no shared session active.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSessionNoSharedSession) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");
  RunFunction(new LoginEndSharedSessionFunction(), "[]");

  ASSERT_EQ(
      login_api_errors::kSharedSessionIsNotActive,
      RunFunctionAndReturnError(new LoginEndSharedSessionFunction(), "[]"));
}

// Test that calling `login.endSharedSession()` returns an error when there
// is a cleanup in progress.
TEST_F(LoginApiSharedSessionUnittest, EndSharedSessionCleanupInProgress) {
  LaunchSharedManagedGuestSession("foo");
  chromeos::CleanupManagerAsh::Get()->SetIsCleanupInProgressForTesting(true);

  ASSERT_EQ(login_api_errors::kCleanupInProgress,
            RunFunctionAndReturnError(new LoginEndSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` with a password sets the user
// hash and salt.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSession) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");
  RunFunction(new LoginEndSharedSessionFunction(), "[]");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/true);

  base::TimeTicks now = base::TimeTicks::Now();

  ui::UserActivityDetector::Get()->set_now_for_test(now);
  RunFunction(new LoginEnterSharedSessionFunction(), "[\"bar\"]");

  chromeos::SharedSessionHandler* handler =
      chromeos::SharedSessionHandler::Get();
  EXPECT_NE("", handler->GetUserSecretHashForTesting());
  EXPECT_NE("", handler->GetUserSecretSaltForTesting());

  // Test that user activity is triggered.
  EXPECT_EQ(now, ui::UserActivityDetector::Get()->last_activity_time());
}

// Test that calling `login.enterSharedSession()` returns an error when the
// DeviceRestrictedManagedGuestSessionEnabled policy is set to false.
TEST_F(LoginApiSharedSessionUnittest,
       EnterSharedSessionRestrictedMGSNotEnabled) {
  GetCrosSettingsHelper()->SetBoolean(
      ash::kDeviceRestrictedManagedGuestSessionEnabled, false);

  ASSERT_EQ(login_api_errors::kDeviceRestrictedManagedGuestSessionNotEnabled,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` returns an error when the
// session is not locked.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSessionNotLocked) {
  ASSERT_EQ(login_api_errors::kSessionIsNotLocked,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` returns an error when there is
// no shared MGS launched.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSessionNoSharedMGSFound) {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  ASSERT_EQ(login_api_errors::kNoSharedMGSFound,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` returns an error when there
// is another shared session present.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSessionAlreadyLaunched) {
  LaunchSharedManagedGuestSession("foo");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(login_api_errors::kSharedSessionAlreadyLaunched,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` returns an error when there is
// an error when unlocking the screen.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSessionUnlockFailed) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");
  RunFunction(new LoginEndSharedSessionFunction(), "[]");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/false);

  ASSERT_EQ(login_api_errors::kUnlockFailure,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test that calling `login.enterSharedSession()` returns an error when there
// is a cleanup in progress.
TEST_F(LoginApiSharedSessionUnittest, EnterSharedSessionCleanupInProgress) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");
  RunFunction(new LoginEndSharedSessionFunction(), "[]");
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  chromeos::CleanupManagerAsh::Get()->SetIsCleanupInProgressForTesting(true);

  ASSERT_EQ(login_api_errors::kCleanupInProgress,
            RunFunctionAndReturnError(new LoginEnterSharedSessionFunction(),
                                      "[\"foo\"]"));
}

// Test the full shared session flow.
TEST_F(LoginApiSharedSessionUnittest, SharedSessionFlow) {
  SetUpCleanupHandlerMocks();
  LaunchSharedManagedGuestSession("foo");

  chromeos::SharedSessionHandler* handler =
      chromeos::SharedSessionHandler::Get();
  std::string foo_hash = handler->GetUserSecretHashForTesting();
  std::string foo_salt = handler->GetUserSecretSaltForTesting();

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ASSERT_EQ(login_api_errors::kAuthenticationFailed,
            RunFunctionAndReturnError(
                base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
                "[\"bar\"]"));

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/true);

  RunFunction(base::MakeRefCounted<LoginUnlockSharedSessionFunction>(),
              "[\"foo\"]");

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  EXPECT_CALL(*mock_lock_handler_, RequestLockScreen()).WillOnce(Return());

  RunFunction(new LoginEndSharedSessionFunction(), "[]");

  EXPECT_EQ("", handler->GetUserSecretHashForTesting());
  EXPECT_EQ("", handler->GetUserSecretSaltForTesting());

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ExpectAuthenticateWithSessionSecret(/*auth_success=*/true);

  RunFunction(new LoginEnterSharedSessionFunction(), "[\"baz\"]");

  const std::string& baz_hash = handler->GetUserSecretHashForTesting();
  const std::string& baz_salt = handler->GetUserSecretSaltForTesting();

  EXPECT_NE("", baz_hash);
  EXPECT_NE("", baz_salt);
  EXPECT_NE(foo_hash, baz_hash);
  EXPECT_NE(foo_salt, baz_salt);
}

class LoginApiExternalLogoutRequestUnittest : public ExtensionApiUnittest {
 public:
  // A mock around the external logout event handler for tracking method calls.
  class MockExternalLogoutRequestEventHandler
      : public ExternalLogoutRequestEventHandler {
   public:
    explicit MockExternalLogoutRequestEventHandler(
        content::BrowserContext* context)
        : ExternalLogoutRequestEventHandler(context) {}
    ~MockExternalLogoutRequestEventHandler() override = default;
    MOCK_METHOD0(OnRequestExternalLogout, void());
  };

  LoginApiExternalLogoutRequestUnittest() = default;

  LoginApiExternalLogoutRequestUnittest(
      const LoginApiExternalLogoutRequestUnittest&) = delete;
  LoginApiExternalLogoutRequestUnittest& operator=(
      const LoginApiExternalLogoutRequestUnittest&) = delete;

  ~LoginApiExternalLogoutRequestUnittest() override = default;

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    mock_external_logout_request_event_handler_ =
        std::make_unique<MockExternalLogoutRequestEventHandler>(profile());
  }

  std::unique_ptr<MockExternalLogoutRequestEventHandler>
      mock_external_logout_request_event_handler_;
};

TEST_F(LoginApiExternalLogoutRequestUnittest, CallsOnRequestExternalLogout) {
  // Expect the |OnRequestExternalLogout()| method to be called.
  EXPECT_CALL(*mock_external_logout_request_event_handler_,
              OnRequestExternalLogout())
      .Times(1);

  auto function = base::MakeRefCounted<LoginRequestExternalLogoutFunction>();
  RunFunction(function.get(), "[]");
}

class LoginApiExternalLogoutDoneUnittest : public ExtensionApiUnittest {
 public:
  class MockExternalLogoutDoneEventHandler
      : public ExternalLogoutDoneEventHandler {
   public:
    explicit MockExternalLogoutDoneEventHandler(
        content::BrowserContext* context)
        : ExternalLogoutDoneEventHandler(context) {}
    ~MockExternalLogoutDoneEventHandler() override = default;
    MOCK_METHOD0(OnExternalLogoutDone, void());
  };

  LoginApiExternalLogoutDoneUnittest() = default;

  LoginApiExternalLogoutDoneUnittest(
      const LoginApiExternalLogoutDoneUnittest&) = delete;
  LoginApiExternalLogoutDoneUnittest& operator=(
      const LoginApiExternalLogoutDoneUnittest&) = delete;

  ~LoginApiExternalLogoutDoneUnittest() override = default;

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    mock_external_logout_done_event_handler_ =
        std::make_unique<MockExternalLogoutDoneEventHandler>(profile());
  }

  void TearDown() override {
    mock_external_logout_done_event_handler_.reset();

    ExtensionApiUnittest::TearDown();
  }

  std::unique_ptr<MockExternalLogoutDoneEventHandler>
      mock_external_logout_done_event_handler_;
};

TEST_F(LoginApiExternalLogoutDoneUnittest, CallsOnExternalLogoutDone) {
  // Expect the |OnExternalLogoutDone()| method to be called.
  EXPECT_CALL(*mock_external_logout_done_event_handler_, OnExternalLogoutDone())
      .Times(1);

  auto function = base::MakeRefCounted<LoginNotifyExternalLogoutDoneFunction>();
  RunFunction(function.get(), "[]");
}

}  // namespace extensions
