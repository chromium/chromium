// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter_factory.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
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

using testing::_;
using testing::Mock;
using testing::Return;
using testing::Sequence;

namespace chromeos {

namespace {
const char kTestUser[] = "user@example.com";
}

class SAMLOfflineSigninLimiterTest : public testing::Test {
 protected:
  SAMLOfflineSigninLimiterTest();
  ~SAMLOfflineSigninLimiterTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void DestroyLimiter();
  void CreateLimiter();

  void SetUpUserManager();

  const AccountId test_account_id_ = AccountId::FromUserEmail(kTestUser);

  TestingPrefServiceSimple* GetTestingLocalState();

  content::BrowserTaskEnvironment task_environment_;
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;

  MockUserManager* user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  base::SimpleTestClock clock_;
  base::MockOneShotTimer* timer_;  // Not owned.

  SAMLOfflineSigninLimiter* limiter_;  // Owned.
  base::PowerMonitorTestSource* power_source_;

  TestingPrefServiceSimple testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(SAMLOfflineSigninLimiterTest);
};

SAMLOfflineSigninLimiterTest::SAMLOfflineSigninLimiterTest()
    : user_manager_(new MockUserManager),
      user_manager_enabler_(base::WrapUnique(user_manager_)),
      timer_(nullptr),
      limiter_(nullptr) {
  auto power_source = std::make_unique<base::PowerMonitorTestSource>();
  power_source_ = power_source.get();
  base::PowerMonitor::Initialize(std::move(power_source));
}

SAMLOfflineSigninLimiterTest::~SAMLOfflineSigninLimiterTest() {
  DestroyLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, Shutdown()).Times(1);
  EXPECT_CALL(*user_manager_, RemoveSessionStateObserver(_)).Times(1);
  profile_.reset();
  // Finish any pending tasks before deleting the TestingBrowserProcess.
  task_environment_.RunUntilIdle();
  TestingBrowserProcess::DeleteInstance();
  base::PowerMonitor::ShutdownForTesting();
}

void SAMLOfflineSigninLimiterTest::DestroyLimiter() {
  if (limiter_) {
    limiter_->Shutdown();
    delete limiter_;
    limiter_ = nullptr;
    timer_ = nullptr;
  }
}

void SAMLOfflineSigninLimiterTest::CreateLimiter() {
  DestroyLimiter();
  limiter_ = new SAMLOfflineSigninLimiter(profile_.get(), &clock_);
  auto timer = std::make_unique<base::MockOneShotTimer>();
  timer_ = timer.get();
  limiter_->SetTimerForTesting(std::move(timer));
}

void SAMLOfflineSigninLimiterTest::SetUpUserManager() {
  EXPECT_CALL(*user_manager_, GetLocalState())
      .WillRepeatedly(Return(GetTestingLocalState()));
}

void SAMLOfflineSigninLimiterTest::SetUp() {
  profile_.reset(new TestingProfile);

  SAMLOfflineSigninLimiterFactory::SetClockForTesting(&clock_);
  user_manager_->AddUser(test_account_id_);
  profile_->set_profile_name(kTestUser);
  clock_.Advance(base::TimeDelta::FromHours(1));

  user_manager_->RegisterPrefs(GetTestingLocalState()->registry());
  SetUpUserManager();
}

TestingPrefServiceSimple* SAMLOfflineSigninLimiterTest::GetTestingLocalState() {
  return &testing_local_state_;
}

void SAMLOfflineSigninLimiterTest::TearDown() {
  SAMLOfflineSigninLimiterFactory::SetClockForTesting(nullptr);
}

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLDefaultLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
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
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLNoLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
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
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLZeroLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
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
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  pref = prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLSetLimitWhileLoggedIn) {
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
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

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLRemoveLimitWhileLoggedIn) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
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

TEST_F(SAMLOfflineSigninLimiterTest, NoSAMLLogInWithExpiredLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Authenticate against GAIA without SAML. Verify that the flag enforcing
  // online login and the time of last login with SAML are cleared.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kSAMLLastGAIASignInTime);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->HasUserSetting());

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLDefaultLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login with SAML are not changed.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Allow the timer to fire. Verify that the flag enforcing online login is
  // set.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(1);
  timer_->Fire();
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLNoLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login with SAML are not changed.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLZeroLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set a zero time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and then set immediately. Also verify that the time of
  // last login with SAML is set.
  CreateLimiter();
  Sequence sequence;
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1)
      .InSequence(sequence);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(1)
      .InSequence(sequence);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLSetLimitWhileLoggedIn) {
  PrefService* prefs = profile_->GetPrefs();

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(1);
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, 0);
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLRemoveLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Allow the timer to fire. Verify that the flag enforcing online login is not
  // changed.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLLogInWithExpiredLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is updated.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  const base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLLogInOfflineWithExpiredLimit) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Advance time by four weeks.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Authenticate offline. Verify that the flag enforcing online login is
  // set and the time of last login with SAML is not changed.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(1);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  const base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);
}

TEST_F(SAMLOfflineSigninLimiterTest, SAMLLimitExpiredWhileSuspended) {
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                  clock_.Now().ToInternalValue());

  // Authenticate against GAIA with SAML. Verify that the flag enforcing online
  // login is cleared and the time of last login with SAML is set.
  CreateLimiter();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  // Suspend for 4 weeks.
  power_source_->GenerateSuspendEvent();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_account_id_, true))
      .Times(1);
  power_source_->GenerateResumeEvent();
}

}  //  namespace chromeos
