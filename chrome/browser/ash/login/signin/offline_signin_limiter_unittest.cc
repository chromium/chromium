// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/profiles/profile.h"
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

const char kTestGaiaUser[] = "user@example.com";
const char kTestSAMLUser[] = "user@saml.example.com";

}  // namespace

class OfflineSigninLimiterTest : public testing::Test {
 protected:
  OfflineSigninLimiterTest();
  ~OfflineSigninLimiterTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void DestroyLimiter();
  void CreateLimiter();

  void SetUpUserManager();
  void AddGaiaUser();
  void AddSAMLUser();

  const AccountId test_gaia_account_id_ =
      AccountId::FromUserEmail(kTestGaiaUser);
  const AccountId test_saml_account_id_ =
      AccountId::FromUserEmail(kTestSAMLUser);

  TestingPrefServiceSimple* GetTestingLocalState();

  content::BrowserTaskEnvironment task_environment_;
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;

  MockUserManager* user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  base::SimpleTestClock clock_;
  base::MockOneShotTimer* timer_;  // Not owned.

  OfflineSigninLimiter* limiter_;  // Owned.
  base::PowerMonitorTestSource* power_source_;

  TestingPrefServiceSimple testing_local_state_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OfflineSigninLimiterTest);
};

OfflineSigninLimiterTest::OfflineSigninLimiterTest()
    : user_manager_(new MockUserManager),
      user_manager_enabler_(base::WrapUnique(user_manager_)),
      timer_(nullptr),
      limiter_(nullptr) {
  feature_list_.InitAndEnableFeature(
      features::kEnableSamlReauthenticationOnLockscreen);
  auto power_source = std::make_unique<base::PowerMonitorTestSource>();
  power_source_ = power_source.get();
  base::PowerMonitor::Initialize(std::move(power_source));
}

OfflineSigninLimiterTest::~OfflineSigninLimiterTest() {
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

void OfflineSigninLimiterTest::DestroyLimiter() {
  if (limiter_) {
    limiter_->Shutdown();
    delete limiter_;
    limiter_ = nullptr;
    timer_ = nullptr;
  }
}

void OfflineSigninLimiterTest::CreateLimiter() {
  DestroyLimiter();
  limiter_ = new OfflineSigninLimiter(profile_.get(), &clock_);
  auto timer = std::make_unique<base::MockOneShotTimer>();
  timer_ = timer.get();
  limiter_->SetTimerForTesting(std::move(timer));
}

void OfflineSigninLimiterTest::SetUpUserManager() {
  EXPECT_CALL(*user_manager_, GetLocalState())
      .WillRepeatedly(Return(GetTestingLocalState()));
}

void OfflineSigninLimiterTest::SetUp() {
  profile_.reset(new TestingProfile);

  OfflineSigninLimiterFactory::SetClockForTesting(&clock_);
  clock_.Advance(base::TimeDelta::FromHours(1));
}

void OfflineSigninLimiterTest::AddGaiaUser() {
  user_manager_->AddUser(test_gaia_account_id_);
  profile_->set_profile_name(kTestGaiaUser);

  user_manager_->RegisterPrefs(GetTestingLocalState()->registry());
  SetUpUserManager();
}

void OfflineSigninLimiterTest::AddSAMLUser() {
  user_manager_->AddPublicAccountWithSAML(test_saml_account_id_);
  profile_->set_profile_name(kTestSAMLUser);

  user_manager_->RegisterPrefs(GetTestingLocalState()->registry());
  SetUpUserManager();
}

TestingPrefServiceSimple* OfflineSigninLimiterTest::GetTestingLocalState() {
  return &testing_local_state_;
}

void OfflineSigninLimiterTest::TearDown() {
  OfflineSigninLimiterFactory::SetClockForTesting(nullptr);
}

TEST_F(OfflineSigninLimiterTest, NoSAMLDefaultLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  SetUpUserManager();
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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  SetUpUserManager();
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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  SetUpUserManager();
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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
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
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Allow the timer to fire. Verify that the flag enforcing online login is
  // set.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  timer_->Fire();
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
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITH_SAML);

  last_gaia_signin_time = prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit, -1);

  // Allow the timer to fire. Verify that the flag enforcing online login is not
  // changed.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_saml_account_id_, _))
      .Times(0);
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInWithExpiredLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInOfflineWithExpiredLimit) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

  // Advance time by four weeks.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());

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
  power_source_->GenerateSuspendEvent();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_saml_account_id_, true))
      .Times(1);
  power_source_->GenerateResumeEvent();
}

TEST_F(OfflineSigninLimiterTest, SAMLLogInOfflineWithOnLockReauth) {
  AddSAMLUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last login with SAML and time limit.
  prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_.Now());
  prefs->SetInteger(prefs::kSAMLOfflineSigninTimeLimit,
                    base::TimeDelta::FromDays(1).InSeconds());  // 1 day.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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

TEST_F(OfflineSigninLimiterTest, NoGaiaDefaultLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed and the time of last login with SAML is not set.
  CreateLimiter();
  SetUpUserManager();
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
  SetUpUserManager();
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
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
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
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login is not
  // changed.
  CreateLimiter();
  SetUpUserManager();
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
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());

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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is updated.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate against Gaia without SAML. Verify that the flag enforcing
  // online login is cleared and the time of last login without SAML is updated.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Log out. Verify that the flag enforcing online login is not set.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_FALSE(timer_->IsRunning());

  // Set a zero time limit. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
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
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Remove the time limit.
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, -1);

  // Allow the timer to fire. Verify that the flag enforcing online login is not
  // changed.
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInWithExpiredLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInOfflineWithExpiredLimit) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and set limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Advance time by four weeks.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
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
  power_source_->GenerateSuspendEvent();
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

  // Resume power. Verify that the flag enforcing online login is set.
  Mock::VerifyAndClearExpectations(user_manager_);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(0);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(1);
  power_source_->GenerateResumeEvent();
}

TEST_F(OfflineSigninLimiterTest, GaiaLogInOfflineWithOnLockReauth) {
  AddGaiaUser();
  PrefService* prefs = profile_->GetPrefs();

  // Set the time of last Gaia login without SAML and time limit.
  prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_.Now());
  prefs->SetInteger(prefs::kGaiaOfflineSigninTimeLimitDays, 7);  // 1 week.

  // Enable re-authentication on the lock screen.
  prefs->SetBoolean(prefs::kLockScreenReauthenticationEnabled, true);

  // Advance time by four weeks.
  clock_.Advance(base::TimeDelta::FromDays(28));  // 4 weeks.

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
  SetUpUserManager();
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, false))
      .Times(1);
  EXPECT_CALL(*user_manager_,
              SaveForceOnlineSignin(test_gaia_account_id_, true))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(clock_.Now(), last_gaia_signin_time);

  // Verify that no timer is running.
  EXPECT_TRUE(timer_->IsRunning());

  // Log out.
  DestroyLimiter();

  // Advance time by an hour.
  const base::Time gaia_signin_time = clock_.Now();
  clock_.Advance(base::TimeDelta::FromHours(1));

  // Authenticate offline. Verify that the flag enforcing online login and the
  // time of last login without SAML are not changed.
  CreateLimiter();
  Mock::VerifyAndClearExpectations(user_manager_);
  SetUpUserManager();
  EXPECT_CALL(*user_manager_, SaveForceOnlineSignin(test_gaia_account_id_, _))
      .Times(0);
  limiter_->SignedIn(UserContext::AUTH_FLOW_OFFLINE);

  last_gaia_signin_time = prefs->GetTime(prefs::kGaiaLastOnlineSignInTime);
  EXPECT_EQ(gaia_signin_time, last_gaia_signin_time);

  // Verify that the timer is running.
  EXPECT_TRUE(timer_->IsRunning());
}

}  //  namespace chromeos
