// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/ui/mock_login_display.h"
#include "chrome/browser/chromeos/login/ui/mock_login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::ReturnNull;

namespace chromeos {

namespace {

const char kFirstSAMLUserId[] = "12345";
const char kFirstSAMLUserEmail[] = "bob@corp.example.com";
const char kSecondSAMLUserId[] = "67891";
const char kSecondSAMLUserEmail[] = "alice@corp.example.com";

const char kFirstGaiaUserId[] = "88888";
const char kFirstGaiaUserEmail[] = "bob@gaia.example.com";
const char kSecondGaiaUserId[] = "88884";
const char kSecondGaiaUserEmail[] = "alice@gaia.example.com";

const char kSamlToken1[] = "saml-token-1";
const char kSamlToken2[] = "saml-token-2";

constexpr base::TimeDelta kLoginOnlineShortDelay =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kLoginOnlineLongDelay =
    base::TimeDelta::FromSeconds(100);
constexpr base::TimeDelta kLoginOnlineVeryLongDelay =
    base::TimeDelta::FromSeconds(1000);
constexpr base::TimeDelta kLoginOnlineOffset = base::TimeDelta::FromSeconds(1);

class FakeUserManagerWithLocalState : public chromeos::FakeChromeUserManager {
 public:
  explicit FakeUserManagerWithLocalState(MockUserManager* mock_user_manager)
      : mock_user_manager_(mock_user_manager),
        test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

  MockUserManager* mock_user_manager() { return mock_user_manager_; }

 private:
  // Unowned pointer.
  MockUserManager* const mock_user_manager_;
  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;
};

}  // namespace

class ExistingUserControllerForcedOnlineAuthTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_login_display_host_ = std::make_unique<MockLoginDisplayHost>();
    mock_login_display_ = std::make_unique<MockLoginDisplay>();
    mock_user_manager_ = new MockUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>(mock_user_manager_));
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    existing_user_controller_ = std::make_unique<ExistingUserController>();

    ON_CALL(*mock_login_display_host_, GetLoginDisplay())
        .WillByDefault(Return(mock_login_display_.get()));
    ON_CALL(*mock_login_display_host_, GetExistingUserController())
        .WillByDefault(Return(existing_user_controller_.get()));
  }

  ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  // ExistingUserController private member accessors.
  base::OneShotTimer* screen_refresh_timer() {
    return existing_user_controller()->screen_refresh_timer_.get();
  }

  bool is_force_online_flag_set() {
    return existing_user_controller()->ForceOnlineFlagChanged(
        mock_user_manager()->GetUsers());
  }

  MockUserManager* mock_user_manager() { return mock_user_manager_; }

  int password_sync_token_checkers_size() {
    if (!existing_user_controller()->sync_token_checkers_)
      return 0;
    return existing_user_controller()
        ->sync_token_checkers_->sync_token_checkers_.size();
  }

  PasswordSyncTokenLoginChecker* get_password_sync_token_checker(
      std::string token) {
    return existing_user_controller()
        ->sync_token_checkers_->sync_token_checkers_[token]
        .get();
  }

  void set_hide_user_names_on_signin() {
    settings_helper_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, false);
  }

  const AccountId saml_login_account1_id_ =
      AccountId::FromUserEmailGaiaId(kFirstSAMLUserEmail, kFirstSAMLUserId);

  const AccountId saml_login_account2_id_ =
      AccountId::FromUserEmailGaiaId(kSecondSAMLUserEmail, kSecondSAMLUserId);

  const AccountId gaia_login_account1_id_ =
      AccountId::FromUserEmailGaiaId(kFirstGaiaUserEmail, kFirstGaiaUserId);

  const AccountId gaia_login_account2_id_ =
      AccountId::FromUserEmailGaiaId(kSecondGaiaUserEmail, kSecondGaiaUserId);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<MockLoginDisplayHost> mock_login_display_host_;
  std::unique_ptr<MockLoginDisplay> mock_login_display_;

  // Required by ExistingUserController:
  ScopedCrosSettingsTestHelper settings_helper_;
  ArcKioskAppManager arc_kiosk_app_manager_;
  MockUserManager* mock_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;
};

// Tests login screen update when SAMLOfflineSigninTimeLimit policy is set.
TEST_F(ExistingUserControllerForcedOnlineAuthTest, SamlOnlineAuthSingleUser) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  // Check timer again 1s after its expacted expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_TRUE(is_force_online_flag_set());
}

// Verfies that `OfflineSigninLimiter` does affect SAML and non SAML user.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       OfflineLimiteOutOfSessionSAMLAndNonSAML) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(saml_login_account1_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  // Expect true due user has value in `kOfflineSigninLimit`.
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  EXPECT_FALSE(is_force_online_flag_set());
}

// Tests login screen update functionality for 2 SAML users.
TEST_F(ExistingUserControllerForcedOnlineAuthTest, SamlOnlineAuthTwoSamlUsers) {
  base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineLongDelay);

  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account2_id_,
                                                  kLoginOnlineVeryLongDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  // The timer should be re-started after
  // (kLoginOnlineLongDelay - kLoginOnlineShortDelay) s.
  task_environment_.FastForwardBy(kLoginOnlineLongDelay -
                                  kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
}

// Tests login screen update functionality for 2 users: SAML and non-SAML.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SamlOnlineAuthSamlAndNonSamlUsers) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetLastOnlineSignin(saml_login_account2_id_, now);

  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  kLoginOnlineShortDelay);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account2_id_,
                                                  kLoginOnlineLongDelay);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddUser(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  EXPECT_TRUE(is_force_online_flag_set());
}

// Tests unset policy value in local state.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SamlOnlineAuthSamlPolicyNotSet) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(saml_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(saml_login_account1_id_,
                                                  base::nullopt);

  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_FALSE(is_force_online_flag_set());
}

// Tests login screen update when `GaiaOfflineSigninTimeLimitDays` policy is set
// and the last online sign in has not been set. This is the case for those
// devices that went through the online signin in the first login before the
// introduction of `GaiaOfflineSigninTimeLimitDays` policy logic which didn't
// store the last online sign in.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       GaiaOnlineAuthSingleUserNoLastOnlineSignin) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  // Since `LastOnlinesignin` value is null and there is a limit, it will
  // enforce the next login to be online. No timer should be running.
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_TRUE(is_force_online_flag_set());

  // User logged in online after enforcement.
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  // Check timer again 1s after its expacted expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_TRUE(is_force_online_flag_set());
}

// Tests login screen update when `GaiaOfflineSigninTimeLimitDays` policy is set
// and the last online sign in has been set.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       GaiaOnlineAuthSingleUserLastOnlineSignin) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineShortDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  // Check timer again 1s after its expacted expiry.
  task_environment_.FastForwardBy(kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_TRUE(is_force_online_flag_set());
}

// Tests login screen update functionality for 2 Gaia without SAML users.
TEST_F(ExistingUserControllerForcedOnlineAuthTest, GaiaOnlineAuthTwoGaiaUsers) {
  base::Time now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account1_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  kLoginOnlineLongDelay);

  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  now = base::DefaultClock::GetInstance()->Now();
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account2_id_,
                                                  kLoginOnlineVeryLongDelay);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  mock_user_manager()->AddUser(gaia_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
  // The timer should be re-started after
  // (kLoginOnlineLongDelay - kLoginOnlineShortDelay) s.
  task_environment_.FastForwardBy(kLoginOnlineLongDelay -
                                  kLoginOnlineShortDelay + kLoginOnlineOffset);
  EXPECT_TRUE(screen_refresh_timer()->IsRunning());
}

// Tests unset `GaiaOfflineTimeLimitDays` policy value in local state.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       GaiaOnlineAuthGaiaPolicyNotSet) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  // No `LastOnlineSignin` value, case where devices didn't store that value in
  // the first Gaia login.
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account1_id_,
                                                  base::nullopt);

  // Case where the user has already stored last online signin.
  user_manager::known_user::SetLastOnlineSignin(gaia_login_account2_id_, now);
  user_manager::known_user::SetOfflineSigninLimit(gaia_login_account2_id_,
                                                  base::nullopt);

  mock_user_manager()->AddUser(gaia_login_account1_id_);
  mock_user_manager()->AddUser(gaia_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_FALSE(screen_refresh_timer()->IsRunning());
  EXPECT_FALSE(is_force_online_flag_set());
}

// Tests creation of password sync token checker for 2 SAML users. Only one of
// them has local copy of password sync token.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersCreationWithOneToken) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 1);
  get_password_sync_token_checker(kSamlToken1)->OnTokenVerified(true);
  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  EXPECT_TRUE(get_password_sync_token_checker(kSamlToken1)->IsCheckPending());
}

// Tests creation of password sync token checker for 2 SAML users with password
// sync tokens.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersCreationWithTwoTokens) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 2);
  get_password_sync_token_checker(kSamlToken1)
      ->OnApiCallFailed(PasswordSyncTokenFetcher::ErrorType::kServerError);
  task_environment_.FastForwardBy(kLoginOnlineShortDelay);
  EXPECT_TRUE(get_password_sync_token_checker(kSamlToken1)->IsCheckPending());
}

// Tests sync token checkers removal in case of failed token validation.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       SyncTokenCheckersInvalidPasswordForTwoUsers) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  set_hide_user_names_on_signin();
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 2);
  get_password_sync_token_checker(kSamlToken1)->OnTokenVerified(false);
  get_password_sync_token_checker(kSamlToken2)->OnTokenVerified(false);
  EXPECT_EQ(password_sync_token_checkers_size(), 0);
}

// Sync token checkers are not owned by ExistingUserController if user pods are
// visible.
TEST_F(ExistingUserControllerForcedOnlineAuthTest,
       NoSyncTokenCheckersWhenPodsVisible) {
  user_manager::known_user::SetPasswordSyncToken(saml_login_account1_id_,
                                                 kSamlToken1);
  user_manager::known_user::SetPasswordSyncToken(saml_login_account2_id_,
                                                 kSamlToken2);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account1_id_);
  mock_user_manager()->AddPublicAccountWithSAML(saml_login_account2_id_);
  existing_user_controller()->Init(mock_user_manager()->GetUsers());
  EXPECT_EQ(password_sync_token_checkers_size(), 0);
}

}  // namespace chromeos
