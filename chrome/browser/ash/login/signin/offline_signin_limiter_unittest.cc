// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_login_pref_names.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/login_constants.h"
#include "chrome/browser/ash/login/saml/mock_lock_handler.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestGaiaUser[] = "user@example.com";
constexpr char kTestSAMLUser[] = "user@saml.example.com";
constexpr char kTestPassword[] = "password";

void AddOnlinePassword(const AccountId& user) {
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
      cryptohome::CreateAccountIdentifierFromAccountId(user));
  Key key(kTestPassword);
  user_data_auth::AuthFactor auth_factor;
  user_data_auth::AuthInput auth_input;

  auth_factor.set_label(kCryptohomeGaiaKeyLabel);
  auth_factor.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);

  auth_input.mutable_password_input()->set_secret(key.GetSecret());

  // Add the password key to the user.
  FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
      cryptohome::CreateAccountIdentifierFromAccountId(user), auth_factor,
      auth_input);
}

}  // namespace

class OfflineSigninLimiterTest : public testing::TestWithParam<bool> {
 public:
  OfflineSigninLimiterTest(const OfflineSigninLimiterTest&) = delete;
  OfflineSigninLimiterTest& operator=(const OfflineSigninLimiterTest&) = delete;

 protected:
  OfflineSigninLimiterTest();
  ~OfflineSigninLimiterTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void DestroyLimiter();
  void CreateLimiter();
  void SetLastOnlineSignIn(user_manager::User* user);
  void VerifyLastSignIn(user_manager::User* user, base::Time time);

  FakeChromeUserManager* GetFakeChromeUserManager();

  user_manager::User* AddGaiaUser();
  user_manager::User* AddSAMLUser();

  void LockScreen();
  void UnlockScreen();
  void CheckAuthTypeOnLock(AccountId account_id, bool expect_online_auth);

  const AccountId test_gaia_account_id_ =
      AccountId::FromUserEmail(kTestGaiaUser);
  const AccountId test_saml_account_id_ =
      AccountId::FromUserEmail(kTestSAMLUser);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<TestingProfile> profile_;

  MockLockHandler lock_handler_;

  raw_ptr<base::WallClockTimer, DanglingUntriaged> timer_ = nullptr;

  std::unique_ptr<OfflineSigninLimiter> limiter_;
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;

  std::unique_ptr<user_manager::KnownUser> known_user_;
  std::optional<session_manager::SessionManager> session_manager_;
  base::test::ScopedFeatureList features_;
};

OfflineSigninLimiterTest::OfflineSigninLimiterTest() {
  if (GetParam()) {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kManagedLocalPinAndPassword,
                              features::kRecoveryFlowReorder},
        /*disabled_features=*/{});
  } else {
    features_.InitWithFeatures(/*enabled_features=*/{}, /*disabled_features=*/{
                                   features::kManagedLocalPinAndPassword,
                                   features::kRecoveryFlowReorder});
  }
}

OfflineSigninLimiterTest::~OfflineSigninLimiterTest() {
  // Finish any pending tasks before deleting the TestingBrowserProcess.
  task_environment_.RunUntilIdle();
}

void OfflineSigninLimiterTest::DestroyLimiter() {
  if (limiter_) {
    limiter_->Shutdown();
    limiter_.reset();
    timer_ = nullptr;
  }
}

void OfflineSigninLimiterTest::CreateLimiter() {
  DCHECK(profile_.get());

  DestroyLimiter();
  // OfflineSigninLimiter has a private constructor.
  limiter_ = base::WrapUnique(new OfflineSigninLimiter(
      TestingBrowserProcess::GetGlobal()->local_state(), profile_.get(),
      task_environment_.GetMockClock()));
  timer_ = limiter_->GetTimerForTesting();
}

void OfflineSigninLimiterTest::SetLastOnlineSignIn(user_manager::User* user) {
  known_user_->SetLastOnlineSignin(user->GetAccountId(),
                                   task_environment_.GetMockClock()->Now());
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetTime(prefs::kLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
}

// Verifies that timestamp in both local state and prefs matches the expected.
void OfflineSigninLimiterTest::VerifyLastSignIn(user_manager::User* user,
                                                base::Time time) {
  EXPECT_EQ(time, known_user_->GetLastOnlineSignin(user->GetAccountId()));
  EXPECT_EQ(time, profile_->GetPrefs()->GetTime(prefs::kLastOnlineSignInTime));
}

void OfflineSigninLimiterTest::SetUp() {
  FakeUserDataAuthClient::InitializeFake();
  session_manager_.emplace(
      std::make_unique<session_manager::FakeSessionManagerDelegate>());
  fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  profile_ = std::make_unique<TestingProfile>();
  known_user_ = std::make_unique<user_manager::KnownUser>(
      TestingBrowserProcess::GetGlobal()->local_state());
  FakeUserDataAuthClient::TestApi::Get()->CreatePostponedDirectories();
}

void OfflineSigninLimiterTest::TearDown() {
  DestroyLimiter();
  profile_.reset();
  session_manager_.reset();
  fake_user_manager_.Reset();
  FakeUserDataAuthClient::Shutdown();
}

FakeChromeUserManager* OfflineSigninLimiterTest::GetFakeChromeUserManager() {
  return fake_user_manager_.Get();
}

user_manager::User* OfflineSigninLimiterTest::AddGaiaUser() {
  auto* user = fake_user_manager_->AddUser(test_gaia_account_id_);
  profile_->set_profile_name(kTestGaiaUser);
  fake_user_manager_->UserLoggedIn(
      user->GetAccountId(),
      user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  AddOnlinePassword(user->GetAccountId());
  return user;
}

user_manager::User* OfflineSigninLimiterTest::AddSAMLUser() {
  auto* user = fake_user_manager_->AddSamlUser(test_saml_account_id_);
  profile_->set_profile_name(kTestSAMLUser);
  fake_user_manager_->UserLoggedIn(
      user->GetAccountId(),
      user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  AddOnlinePassword(user->GetAccountId());
  return user;
}

void OfflineSigninLimiterTest::LockScreen() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
}

void OfflineSigninLimiterTest::UnlockScreen() {
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

// Check that correct auth type is set when the screen is locked.
void OfflineSigninLimiterTest::CheckAuthTypeOnLock(AccountId account_id,
                                                   bool expect_online_auth) {
  // Locking the screen will result in checking whether or not online
  // reauth is required. `SetAuthType` will be called if and only if online
  // reauth is required. Note that due to quirks of implementation it can be
  // called more than once when its required (`OfflineSigninLimiter` and
  // `LockScreenReauthManager` both monitor session state which can result in
  // two calls).
  EXPECT_CALL(
      lock_handler_,
      SetAuthType(account_id, proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
                  std::u16string()))
      .Times(expect_online_auth ? testing::AtLeast(1) : testing::Exactly(0));

  LockScreen();
  // After locking the screen, execute any pending asynchronous operations to
  // allow UserDataAuthClient calls to resolve.
  task_environment_.RunUntilIdle();
  // Simulate unlock to allow calling tests to modify policies and call
  // `CheckAuthTypeOnLock` again.
  UnlockScreen();
}

TEST_P(OfflineSigninLimiterTest, NoGaiaDefaultLimit) {
  auto* user = AddGaiaUser();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last online sign-in is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());
  VerifyLastSignIn(user, base::Time());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoGaiaNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last online sign-in is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());
  VerifyLastSignIn(user, base::Time());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoGaiaZeroLimitWhenOffline) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Remove time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate against Gaia with SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out.
  DestroyLimiter();

  // Advance clock by 1 hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoGaiaSetLimitWhileLoggedIn) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);
  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against Gaia with SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that timer is running due to Gaia log in with SAML.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Remove the time limit from SAML.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaDefaultLimit) {
  auto* user = AddGaiaUser();

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last online sign-in are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);

  // Verify that no the timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last online sign-in are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaZeroLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is set. Also verify that the time of last login without
  // SAML is set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_TRUE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());
}

TEST_P(OfflineSigninLimiterTest, GaiaSetLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);
  EXPECT_TRUE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, GaiaRemoveLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  SetLastOnlineSignIn(user);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  EXPECT_FALSE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, GaiaLogInWithExpiredLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia online sign-in and set limit.
  SetLastOnlineSignIn(user);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaLogInOfflineWithExpiredLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline. Verify that the flag enforcing online login is
  // set and the time of last online sign-in is not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaLimitExpiredWhileSuspended) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of Gaia last online sign-in and set time limit.
  SetLastOnlineSignIn(user);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Suspend for 4 weeks.
  test_power_monitor_source_.Suspend();
  task_environment_.AdvanceClock(base::Days(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  test_power_monitor_source_.Resume();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, GaiaLogInOfflineWithOnLockReauth) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  // After changing the re-auth flag timer should be stopped.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthNoLimit) {
  // Test that the gaia only user is not forced to reauthenticate on the
  // lockscreen after some time has passed.

  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with only Gaia.
  SetLastOnlineSignIn(user);

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  CheckAuthTypeOnLock(test_gaia_account_id_, false /*expect_online_auth*/);
}

TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthZeroLimit) {
  // Test that the gaia only user is required to go through online
  // reauthentication on the lock screen every time the screen is locked.

  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with only Gaia
  SetLastOnlineSignIn(user);

  // Set a zero time limit which requires online reauth every time.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays, 0);

  // Create limiter instance
  CreateLimiter();

  // Advance time by 4 days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate online and check that lockscreen timer is not running as
  // reauthentication is required.
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);

  // Advance time by 2 hours.
  task_environment_.FastForwardBy(base::Hours(2));

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);
}

TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthWithLimit) {
  // Test that the gaia only user is required to go through online
  // reauthentication on the lock screen when the time limit for the lockscreen
  // has passed.

  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with only Gaia
  SetLastOnlineSignIn(user);

  // Add reauth time limit of 2 weeks.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays, 14);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate offline and check if lockscreen timer is still running.
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(limiter_->GetLockscreenTimerForTesting()->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);
}

// Test that the lockscreen time limit matches the login time limit which is
// done by giving lockscreen limit a value of -2 and this will make
// "kGaiaLockScreenOfflineSigninTimeLimitDays" get the value set for
// "kGaiaOfflineSigninTimeLimitDays"
// ---------------------------------------------------
// Test when login limit is not set (policy value = -1)
// ---------------------------------------------------
TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthMatchLoginNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with only Gaia
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to -1 (no limit).
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now -1.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  CheckAuthTypeOnLock(test_gaia_account_id_, false /*expect_online_auth*/);
}

// ---------------------------------------------------
// Test when login limit is Zero
// ---------------------------------------------------
TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthMatchLoginZeroLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with only Gaia
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to 0.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now zero.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by 4 days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate online and check that lockscreen timer is not running as
  // reauthentication is required.
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);

  // Advance time by 2 hours.
  task_environment_.FastForwardBy(base::Hours(2));

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);
}

// -------------------------------------------------------------
// Test when login limit is 14 days (reauth every 2 weeks)
// -------------------------------------------------------------
TEST_P(OfflineSigninLimiterTest, GaiaLockscreenReauthMatchLoginWithLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to 14.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 14);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now 14.
  prefs->SetInteger(prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate offline and check if lockscreen timer is still running.
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(limiter_->GetLockscreenTimerForTesting()->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  CheckAuthTypeOnLock(test_gaia_account_id_, true /*expect_online_auth*/);
}

TEST_P(OfflineSigninLimiterTest, NoSAMLDefaultLimit) {
  auto* user = AddGaiaUser();

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoSAMLNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoSAMLZeroLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoSAMLSetLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoSAMLRemoveLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, NoSAMLLogInWithExpiredLimit) {
  auto* user = AddGaiaUser();

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login is cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, SAMLDefaultLimit) {
  auto* user = AddSAMLUser();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last online sign-in are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.
  EXPECT_TRUE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, SAMLNoLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last online sign-in are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, SAMLZeroLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is set. Also verify that the time of last online sign-in is set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_TRUE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());
}

TEST_P(OfflineSigninLimiterTest, SAMLSetLimitWhileLoggedIn) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that no timer is running.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);
  EXPECT_TRUE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, SAMLRemoveLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  EXPECT_FALSE(user->force_online_signin());
  // TODO: check timer_ condition here.
}

TEST_P(OfflineSigninLimiterTest, SAMLLogInWithExpiredLimit) {
  auto* user = AddSAMLUser();

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  // Verify that the timer is running.
  EXPECT_TRUE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, SAMLLogInOfflineWithExpiredLimit) {
  auto* user = AddSAMLUser();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  // Advance time by four weeks.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline. Verify that the flag enforcing online login is
  // set and the time of last online sign-in is not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(user->force_online_signin());

  VerifyLastSignIn(user, gaia_signin_time);
}

TEST_P(OfflineSigninLimiterTest, SAMLLimitExpiredWhileSuspended) {
  auto* user = AddSAMLUser();

  // Set the time of last online sign-in.
  SetLastOnlineSignIn(user);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last online sign-in is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  // Suspend for 4 weeks.
  test_power_monitor_source_.Suspend();
  task_environment_.AdvanceClock(base::Days(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  test_power_monitor_source_.Resume();
  // On resume, the task from the timer need to be finished.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(user->force_online_signin());
}

TEST_P(OfflineSigninLimiterTest, SAMLLogInOfflineWithOnLockReauth) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    base::Days(1).InSeconds());  // 1 day.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  // After changing the re-auth flag timer should be stopped.
  EXPECT_FALSE(limiter_->GetTimerForTesting()->IsRunning());
}

TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthNoLimit) {
  // Test that the saml user is not forced to reauthenticate on the lockscreen
  // after some time has passed.

  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  // Remove the time limit.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  CheckAuthTypeOnLock(test_saml_account_id_, false /*expect_online_auth*/);
}

TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthZeroLimit) {
  // Test that the saml user is required to go through online reauthentication
  // on the lock screen every time the screen is locked.

  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online sign-in
  SetLastOnlineSignIn(user);

  // Set a zero time limit which requires online reauth every time.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays, 0);

  // Create limiter instance
  CreateLimiter();

  // Advance time by 4 days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate online and check that lockscreen timer is not running as
  // reauthentication is required.
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);

  // Advance time by 2 hours.
  task_environment_.FastForwardBy(base::Hours(2));

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);
}

TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthWithLimit) {
  // Test that the saml user is required to go through online reauthentication
  // on the lock screen when the time limit for the lockscreen has passed.

  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  // Add reauth time limit of 2 weeks.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays, 14);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate offline and check if lockscreen timer is still running.
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(limiter_->GetLockscreenTimerForTesting()->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);
}

// Test that the lockscreen time limit matches the login time limit which is
// done by giving lockscreen limit a value of -2 and this will make
// "kSamlLockScreenOfflineSigninTimeLimitDays" get the value set for
// "kSAMLOfflineSigninTimeLimit"
// ---------------------------------------------------
// Test when login limit is not set (policy value = -1)
// ---------------------------------------------------
TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthMatchLoginNoLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online sign-in
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to -1 (no limit).
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    constants::kOfflineSigninTimeLimitNotSet);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now -1.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  CheckAuthTypeOnLock(test_saml_account_id_, false /*expect_online_auth*/);
}

// ---------------------------------------------------
// Test when login limit is Zero
// ---------------------------------------------------
TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthMatchLoginZeroLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online sign-in
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to 0.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now zero.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by 4 days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate online and check that lockscreen timer is not running as
  // reauthentication is required.
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  VerifyLastSignIn(user, task_environment_.GetMockClock()->Now());

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);

  // Advance time by 2 hours.
  task_environment_.FastForwardBy(base::Hours(2));

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);
}

// -------------------------------------------------------------
// Test when login limit is 14 days (reauth every 2 weeks)
// -------------------------------------------------------------
TEST_P(OfflineSigninLimiterTest, SAMLLockscreenReauthMatchLoginWithLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last online login
  SetLastOnlineSignIn(user);

  // Set time limit of loginscreen to 14.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 14);

  // Set a value of -2 which matches the lockscreen limit to the login limit
  // which is now 14.
  prefs->SetInteger(prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
                    constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin);

  // Create limiter instance
  CreateLimiter();

  // Advance time by four days.
  task_environment_.FastForwardBy(base::Days(4));

  // Authenticate offline and check if lockscreen timer is still running.
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(limiter_->GetLockscreenTimerForTesting()->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  CheckAuthTypeOnLock(test_saml_account_id_, true /*expect_online_auth*/);
}

INSTANTIATE_TEST_SUITE_P(OfflineSigninLimiterTestInstantiation,
                         OfflineSigninLimiterTest,
                         ::testing::Bool());

}  //  namespace ash
