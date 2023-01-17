// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Sequence;

const char kTestGaiaUser[] = "user@example.com";
const char kTestSAMLUser[] = "user@saml.example.com";

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

  void DestroyLimiter();
  void CreateLimiter();

  void AddGaiaUser();
  void AddSAMLUser();

  const AccountId test_gaia_account_id_ =
      AccountId::FromUserEmail(kTestGaiaUser);
  const AccountId test_saml_account_id_ =
      AccountId::FromUserEmail(kTestSAMLUser);

  content::BrowserTaskEnvironment task_environment_;
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;

  MockUserManager* user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  base::WallClockTimer* timer_ = nullptr;  // Not owned.
  base::WallClockTimer* lockscreen_timer_ = nullptr;  // Not owned.

  OfflineSigninLimiter* limiter_ = nullptr;  // Owned.
  base::test::ScopedPowerMonitorTestSource test_power_monitor_source_;

  std::unique_ptr<ScopedTestingLocalState> local_state_;
  base::test::ScopedFeatureList feature_list_;
};

OfflineSigninLimiterTest::OfflineSigninLimiterTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      user_manager_(new MockUserManager),
      user_manager_enabler_(base::WrapUnique(user_manager_)) {
  local_state_ = std::make_unique<ScopedTestingLocalState>(
      TestingBrowserProcess::GetGlobal());
}

OfflineSigninLimiterTest::~OfflineSigninLimiterTest() {
  DestroyLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, Shutdown()).Times(1);
  profile_ = nullptr;
  // Finish any pending tasks before deleting the TestingBrowserProcess.
  task_environment_.RunUntilIdle();
  local_state_.reset();
  TestingBrowserProcess::DeleteInstance();
}

void OfflineSigninLimiterTest::DestroyLimiter() {
  if (limiter_) {
    limiter_->Shutdown();
    delete limiter_;
    limiter_ = nullptr;
    timer_ = nullptr;
    lockscreen_timer_ = nullptr;
  }
}

void OfflineSigninLimiterTest::CreateLimiter() {
  DestroyLimiter();
  limiter_ = new OfflineSigninLimiter(profile_.get(),
                                      task_environment_.GetMockClock());
  timer_ = limiter_->GetTimerForTesting();
  lockscreen_timer_ = limiter_->GetLockscreenTimerForTesting();
}

void OfflineSigninLimiterTest::SetUp() {
  profile_ = std::make_unique<TestingProfile>();
}

void OfflineSigninLimiterTest::AddGaiaUser() {
  user_manager_->AddUser(test_gaia_account_id_);
  profile_->set_profile_name(kTestGaiaUser);
}

void OfflineSigninLimiterTest::AddSAMLUser() {
  user_manager_->AddPublicAccountWithSAML(test_saml_account_id_);
  profile_->set_profile_name(kTestSAMLUser);
}

TEST_F(OfflineSigninLimiterTest, NoGaiaDefaultLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaNoLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kGaiaLastOnlineSignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaZeroLimitWhenOffline) {
  AddSAMLUser();
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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoGaiaSetLimitWhileLoggedIn) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Set the time of last login without SAML.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against Gaia with SAML. Verify that the flag enforcing
  // online login and the time of last login without SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no the timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaNoLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaZeroLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and then set immediately. Also verify that the time
  // of last login without SAML is set.
  CreateLimiter();
  Sequence sequence;
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1)
      .InSequence(sequence);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, GaiaSetLimitWhileLoggedIn) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1);
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 0);
}

TEST_F(OfflineSigninLimiterTest, GaiaRemoveLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Verify that the flag enforcing online login is not changed.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInWithExpiredLimit) {
  AddGaiaUser();
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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInOfflineWithExpiredLimit) {
  AddGaiaUser();
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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(password_sync_manager->IsLockReauthEnabled());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaLimitExpiredWhileSuspended) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of Gaia last login without SAML and set time limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime,
                 task_environment_.GetMockClock()->Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  // Suspend for 4 weeks.
  test_power_monitor_source_.Suspend();
  task_environment_.AdvanceClock(base::Days(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1);
  test_power_monitor_source_.Resume();
  task_environment_.RunUntilIdle();
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
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Authenticate offline. Verify that the flag enforcing online is set due no
  // `last_gaia_signin_time` value.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLDefaultLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLNoLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLZeroLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, NoSAMLSetLimitWhileLoggedIn) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

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
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLDefaultLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  Mock::VerifyAndClearExpectations(user_manager_);
  // Allow the timer to fire. Verify that the flag enforcing online login is
  // set
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.
}

TEST_F(OfflineSigninLimiterTest, SAMLNoLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

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
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLZeroLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and then set immediately. Also verify that the time of
  // last login with SAML is set.
  CreateLimiter();
  Sequence sequence;
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1)
      .InSequence(sequence);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, SAMLSetLimitWhileLoggedIn) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);
}

TEST_F(OfflineSigninLimiterTest, SAMLRemoveLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Verify that the flag enforcing online login is not
  // changed.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInWithExpiredLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Advance time by four weeks.
  task_environment_.FastForwardBy(base::Days(28));  // 4 weeks.

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(task_environment_.GetMockClock()->Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInOfflineWithExpiredLimit) {
  AddSAMLUser();
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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(password_sync_manager->IsLockReauthEnabled());

  const base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);
}

TEST_F(OfflineSigninLimiterTest, SAMLLimitExpiredWhileSuspended) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime,
                 task_environment_.GetMockClock()->Now());

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  // Suspend for 4 weeks.
  test_power_monitor_source_.Suspend();
  task_environment_.AdvanceClock(base::Days(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  test_power_monitor_source_.Resume();
  // On resume, the task from the timer need to be finished.
  task_environment_.RunUntilIdle();
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
