// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestGaiaUser[] = "user@example.com";
constexpr char kTestSAMLUser[] = "user@saml.example.com";

}  // namespace

class OfflineSigninLimiterTest : public testing::Test {
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

  FakeChromeUserManager* GetFakeChromeUserManager();

  user_manager::User* AddGaiaUser();
  user_manager::User* AddSAMLUser();

  const AccountId test_gaia_account_id_ =
      AccountId::FromUserEmail(kTestGaiaUser);
  const AccountId test_saml_account_id_ =
      AccountId::FromUserEmail(kTestSAMLUser);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;

  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<FakeChromeUserManager>()};

  std::unique_ptr<TestingProfile> profile_;
  base::raw_ptr<base::WallClockTimer> timer_ = nullptr;
  base::raw_ptr<base::WallClockTimer> lockscreen_timer_ = nullptr;

  std::unique_ptr<OfflineSigninLimiter> limiter_;
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
};

OfflineSigninLimiterTest::OfflineSigninLimiterTest() = default;

OfflineSigninLimiterTest::~OfflineSigninLimiterTest() {
  // Finish any pending tasks before deleting the TestingBrowserProcess.
  task_environment_.RunUntilIdle();
}

void OfflineSigninLimiterTest::DestroyLimiter() {
  if (limiter_) {
    limiter_->Shutdown();
    limiter_.reset();
    timer_ = nullptr;
    lockscreen_timer_ = nullptr;
  }
}

void OfflineSigninLimiterTest::CreateLimiter() {
  DCHECK(profile_.get());

  DestroyLimiter();
  // OfflineSigninLimiter has a private constructor.
  limiter_ = base::WrapUnique(new OfflineSigninLimiter(
      profile_.get(), task_environment_.GetMockClock()));
  timer_ = limiter_->GetTimerForTesting();
  lockscreen_timer_ = limiter_->GetLockscreenTimerForTesting();
}

void OfflineSigninLimiterTest::SetUp() {
  profile_ = std::make_unique<TestingProfile>();
}

void OfflineSigninLimiterTest::TearDown() {
  DestroyLimiter();
  profile_.reset();
}

FakeChromeUserManager* OfflineSigninLimiterTest::GetFakeChromeUserManager() {
  return static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
}

user_manager::User* OfflineSigninLimiterTest::AddGaiaUser() {
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUser(test_gaia_account_id_);
  profile_->set_profile_name(kTestGaiaUser);
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  return user;
}

user_manager::User* OfflineSigninLimiterTest::AddSAMLUser() {
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddPublicAccountUser(test_saml_account_id_,
                                                  /*with_saml=*/true);
  profile_->set_profile_name(kTestSAMLUser);
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  return user;
}

TEST_F(OfflineSigninLimiterTest, NoGaiaDefaultLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaZeroLimitWhenOffline) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  // Remove time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against Gaia with SAML. Verify that the flag enforcing
  // online login and the time of last login without SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

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
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaSetLimitWhileLoggedIn) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Set the time of last login without SAML.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against Gaia with SAML. Verify that the flag enforcing
  // online login and the time of last login without SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that timer is running due to Gaia log in with SAML.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit from SAML.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaDefaultLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no the timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaZeroLimit) {
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

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, GaiaSetLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);
  EXPECT_TRUE(user->force_online_signin());
}

TEST_F(OfflineSigninLimiterTest, GaiaRemoveLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  EXPECT_FALSE(user->force_online_signin());
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInWithExpiredLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInOfflineWithExpiredLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline. Verify that the flag enforcing online login is
  // set and the time of last login without SAML is not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(user->force_online_signin());

  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(password_sync_manager);
  EXPECT_FALSE(password_sync_manager->IsLockReauthEnabled());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaLimitExpiredWhileSuspended) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of Gaia last login without SAML and set time limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
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

TEST_F(OfflineSigninLimiterTest, GaiaLogInOfflineWithOnLockReauth) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and time limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline and check if InSessionPasswordSyncManager is created.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  // Verify that we enter InSessionPasswordSyncManager::ForceReauthOnLockScreen.
  EXPECT_TRUE(password_sync_manager->IsLockReauthEnabled());
  // After changing the re-auth flag timer should be stopped.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaNoLastOnlineSigninWithLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate offline. Verify that the flag enforcing online is set due no
  // `last_gaia_signin_time` value.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(user->force_online_signin());

  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_TRUE(last_gaia_signin_time.is_null());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out.
  DestroyLimiter();

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLDefaultLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLNoLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLZeroLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLSetLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLRemoveLimitWhileLoggedIn) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLLogInWithExpiredLimit) {
  auto* user = AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLDefaultLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login with SAML are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.
  EXPECT_TRUE(user->force_online_signin());
}

TEST_F(OfflineSigninLimiterTest, SAMLNoLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Hours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login with SAML are not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(user->force_online_signin());

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLZeroLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is set. Also verify that the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_TRUE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, SAMLSetLimitWhileLoggedIn) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);
  EXPECT_TRUE(user->force_online_signin());
}

TEST_F(OfflineSigninLimiterTest, SAMLRemoveLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  EXPECT_FALSE(user->force_online_signin());
  // TODO: check timer_ condition here.
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInWithExpiredLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  EXPECT_FALSE(user->force_online_signin());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInOfflineWithExpiredLimit) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  const base::Time gaia_signin_time = task_environment_.GetMockClock()->Now();
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline. Verify that the flag enforcing online login is
  // set and the time of last login with SAML is not changed.
  CreateLimiter();
  EXPECT_FALSE(user->force_online_signin());
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_TRUE(user->force_online_signin());
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(password_sync_manager->IsLockReauthEnabled());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, SAMLLimitExpiredWhileSuspended) {
  auto* user = AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
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

TEST_F(OfflineSigninLimiterTest, SAMLLogInOfflineWithOnLockReauth) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML and time limit.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    base::Days(1).InSeconds());  // 1 day.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline and check if InSessionPasswordSyncManager is created.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  // Verify that we enter InSessionPasswordSyncManager::ForceReauthOnLockScreen.
  EXPECT_TRUE(password_sync_manager->IsLockReauthEnabled());
  // After changing the re-auth flag timer should be stopped.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLLockscreenReauthDefaultLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML, time limit defaults to -1 which is no
  // limit.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate offline and check if lockscreen timer is not running.
  CreateLimiter();
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  EXPECT_FALSE(lockscreen_timer_->IsRunning());
}

}  //  namespace ash
