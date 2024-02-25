// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

using testing::Eq;

namespace policy {

class ManagedSessionServiceTest : public ::testing::Test,
                                  public ManagedSessionService::Observer {
 protected:
  using SessionState = session_manager::SessionState;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ::ash::SessionManagerClient::InitializeFake();
    session_termination_manager_ =
        std::make_unique<::ash::SessionTerminationManager>();
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    managed_session_service_ =
        std::make_unique<ManagedSessionService>(&test_clock_);
  }

  void TearDown() override {
    managed_session_service_.reset();
    session_termination_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  std::unique_ptr<TestingProfile> CreateProfile(AccountId account_id,
                                                bool is_affiliated,
                                                bool login) {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_manager::UserType::kRegular,
        profile.get());
    if (login) {
      user_manager_->LoginUser(account_id, true);
    }
    return profile;
  }

  std::unique_ptr<TestingProfile> GuestLogin() {
    user_manager::User* const user = user_manager_->AddGuestUser();
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(user->GetAccountId().GetUserEmail());
    auto profile = profile_builder.Build();
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile.get());
    user_manager_->LoginUser(user->GetAccountId(), true);
    return profile;
  }

  ManagedSessionService* managed_session_service() {
    return managed_session_service_.get();
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

  ash::FakeChromeUserManager* user_manager() { return user_manager_; }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

  int ObservedLoginCount() { return observed_login_count_; }

  int ObservedSessionTerminationCount() {
    return observed_session_termination_count_;
  }

  int ObservedKioskLoginFailureCount() {
    return observed_kiosk_login_failure_count_;
  }

  int ObservedUnlockFailureCount() { return observed_unlock_failure_count_; }

  void OnLoginFailure(const ash::AuthFailure& error) override {
    auth_failure_ = error;
  }
  void OnLogin(Profile* profile) override {
    logged_in_ = profile;
    ++observed_login_count_;
  }
  void OnLogout(Profile* profile) override { logged_out_ = profile; }
  void OnSessionTerminationStarted(const user_manager::User*) override {
    ++observed_session_termination_count_;
  }
  void OnLocked() override { locked_ = true; }
  void OnUnlocked() override { unlocked_ = true; }
  void OnUnlockAttempt(const bool success,
                       const session_manager::UnlockType unlock_type) override {
    if (success) {
      observed_unlock_failure_count_ = 0;
      locked_ = false;
      unlocked_ = true;
    } else {
      ++observed_unlock_failure_count_;
      locked_ = true;
      unlocked_ = false;
    }
    unlock_type_ = unlock_type;
  }
  void OnResumeActive(base::Time time) override {
    suspend_time_ = std::make_unique<base::Time>(time);
  }
  void OnKioskLoginFailure() override { ++observed_kiosk_login_failure_count_; }

  ash::AuthFailure auth_failure_ = ash::AuthFailure(ash::AuthFailure::NONE);
  raw_ptr<Profile, DanglingUntriaged> logged_in_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> logged_out_ = nullptr;
  bool locked_ = false;
  bool unlocked_ = false;
  session_manager::UnlockType unlock_type_ =
      session_manager::UnlockType::UNKNOWN;
  std::unique_ptr<base::Time> suspend_time_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  session_manager::SessionManager session_manager_;

  std::unique_ptr<::ash::SessionTerminationManager>
      session_termination_manager_;

  base::SimpleTestClock test_clock_;

  std::unique_ptr<ManagedSessionService> managed_session_service_;

  int observed_login_count_ = 0;

  int observed_session_termination_count_ = 0;

  int observed_kiosk_login_failure_count_ = 0;

  int observed_unlock_failure_count_ = 0;
};

TEST_F(ManagedSessionServiceTest, OnSessionStateChanged) {
  managed_session_service()->AddObserver(this);

  session_manager()->SetSessionState(SessionState::LOCKED);
  session_manager()->SetSessionState(SessionState::ACTIVE);

  EXPECT_TRUE(locked_);
  EXPECT_TRUE(unlocked_);

  session_manager()->SetSessionState(SessionState::LOCKED);

  EXPECT_TRUE(locked_);

  locked_ = false;
  unlocked_ = false;

  session_manager()->SetSessionState(SessionState::LOCKED);
  session_manager()->SetSessionState(SessionState::LOGIN_PRIMARY);

  EXPECT_FALSE(locked_);
  EXPECT_TRUE(unlocked_);

  session_manager()->SetSessionState(SessionState::LOCKED);

  EXPECT_TRUE(locked_);

  locked_ = false;
  unlocked_ = false;

  session_manager()->SetSessionState(SessionState::ACTIVE);

  EXPECT_TRUE(unlocked_);
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedAffiliatedAndPrimary) {
  AccountId affiliated_account_id =
      AccountId::FromUserEmail("user0@managed.com");
  std::unique_ptr<TestingProfile> affiliated_profile = CreateProfile(
      affiliated_account_id, true /* affiliated */, true /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(affiliated_account_id);

  EXPECT_TRUE(affiliated_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedAffiliated) {
  AccountId secondary_account_id =
      AccountId::FromUserEmail("user3@managed.com");
  std::unique_ptr<TestingProfile> secondary_profile = CreateProfile(
      secondary_account_id, true /* affiliated */, false /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(secondary_account_id);

  EXPECT_TRUE(secondary_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedPrimary) {
  AccountId unaffiliated_account_id =
      AccountId::FromUserEmail("user2@managed.com");
  std::unique_ptr<TestingProfile> unaffiliated_profile = CreateProfile(
      unaffiliated_account_id, false /* affiliated */, true /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(unaffiliated_account_id);

  EXPECT_TRUE(unaffiliated_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedGuest) {
  const auto guest_profile = GuestLogin();
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(user_manager::GuestAccountId());

  EXPECT_TRUE(guest_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest,
       OnProfileWillBeDestroyedAffiliatedAndPrimary) {
  AccountId affiliated_account_id =
      AccountId::FromUserEmail("user0@managed.com");
  std::unique_ptr<TestingProfile> affiliated_profile = CreateProfile(
      affiliated_account_id, true /* affiliated */, true /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(affiliated_account_id);
  affiliated_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(affiliated_profile->IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedAffiliated) {
  AccountId secondary_account_id =
      AccountId::FromUserEmail("user3@managed.com");
  std::unique_ptr<TestingProfile> secondary_profile = CreateProfile(
      secondary_account_id, true /* affiliated */, false /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(secondary_account_id);
  secondary_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(secondary_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedPrimary) {
  AccountId unaffiliated_account_id =
      AccountId::FromUserEmail("user2@managed.com");
  std::unique_ptr<TestingProfile> unaffiliated_profile = CreateProfile(
      unaffiliated_account_id, false /* affiliated */, true /* login */);
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(unaffiliated_account_id);
  unaffiliated_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(unaffiliated_profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedGuest) {
  const auto guest_profile = GuestLogin();
  managed_session_service()->AddObserver(this);

  session_manager()->NotifyUserProfileLoaded(user_manager::GuestAccountId());
  guest_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(guest_profile->IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, SuspendDone) {
  managed_session_service()->AddObserver(this);
  test_clock()->SetNow(base::Time::Now());
  base::TimeDelta sleep_duration = base::Hours(2);

  power_manager_client()->SendSuspendDone(sleep_duration);

  EXPECT_EQ(*suspend_time_, test_clock()->Now() - sleep_duration);
}

TEST_F(ManagedSessionServiceTest, RemoveObserver) {
  AccountId account_id = AccountId::FromUserEmail("user0@managed.com");
  std::unique_ptr<TestingProfile> profile =
      CreateProfile(account_id, true /* affiliated */, true /* login */);
  managed_session_service()->AddObserver(this);

  managed_session_service()->RemoveObserver(this);

  session_manager()->SetSessionState(SessionState::LOCKED);
  session_manager()->SetSessionState(SessionState::ACTIVE);
  session_manager()->SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(locked_);
  EXPECT_FALSE(unlocked_);

  session_manager()->NotifyUserProfileLoaded(account_id);
  EXPECT_FALSE(profile->IsSameOrParent(logged_in_));

  profile->MaybeSendDestroyedNotification();
  EXPECT_FALSE(profile->IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, LoginFailure) {
  managed_session_service()->AddObserver(this);

  managed_session_service()->OnAuthFailure(
      ash::AuthFailure(ash::AuthFailure::FailureReason::OWNER_REQUIRED));

  EXPECT_EQ(auth_failure_.reason(),
            ash::AuthFailure::FailureReason::OWNER_REQUIRED);
}

TEST_F(ManagedSessionServiceTest, LoginBeforeCreate) {
  AccountId affiliated_account_id =
      AccountId::FromUserEmail("user0@managed.com");
  std::unique_ptr<TestingProfile> affiliated_profile = CreateProfile(
      affiliated_account_id, true /* affiliated */, true /* login */);

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  ASSERT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(affiliated_profile->IsSameOrParent(logged_in_));

  session_manager()->NotifyUserProfileLoaded(affiliated_account_id);

  EXPECT_EQ(ObservedLoginCount(), 1);

  affiliated_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(affiliated_profile->IsSameOrParent(logged_out_));
  ASSERT_EQ(ObservedSessionTerminationCount(), 0);

  ::ash::SessionTerminationManager::Get()->StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);

  EXPECT_EQ(ObservedSessionTerminationCount(), 1);
}

TEST_F(ManagedSessionServiceTest, GuestLoginBeforeCreate) {
  const auto guest_profile = GuestLogin();

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  EXPECT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(guest_profile->IsSameOrParent(logged_in_));

  ::ash::SessionTerminationManager::Get()->StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);

  EXPECT_EQ(ObservedSessionTerminationCount(), 1);

  guest_profile->MaybeSendDestroyedNotification();

  EXPECT_TRUE(guest_profile->IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, LoggedInProfileNotCreated) {
  const AccountId account_id = AccountId::FromUserEmail("user0@managed.com");
  auto* const user = user_manager()->AddUser(account_id);
  // User logged in but profile is not created.
  user_manager()->LoginUser(account_id, /*set_profile_created_flag=*/false);

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  EXPECT_EQ(ObservedLoginCount(), 0);

  // Simulate user profile loaded.
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName(account_id.GetUserEmail());
  auto profile = profile_builder.Build();
  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                               profile.get());
  user_manager()->SimulateUserProfileLoad(account_id);
  session_manager()->NotifyUserProfileLoaded(account_id);

  ASSERT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(profile->IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, KioskLoginFailure) {
  managed_session_service()->AddObserver(this);

  ASSERT_EQ(ObservedKioskLoginFailureCount(), 0);
  managed_session_service()->OnKioskProfileLoadFailed();

  ASSERT_EQ(ObservedKioskLoginFailureCount(), 1);
}

TEST_F(ManagedSessionServiceTest, OnUnlockScreenAttempt) {
  managed_session_service()->AddObserver(this);
  ASSERT_EQ(ObservedUnlockFailureCount(), 0);

  managed_session_service()->OnUnlockScreenAttempt(
      false, session_manager::UnlockType::PIN);

  ASSERT_EQ(ObservedUnlockFailureCount(), 1);
  EXPECT_TRUE(locked_);
  EXPECT_FALSE(unlocked_);
  EXPECT_THAT(unlock_type_, Eq(session_manager::UnlockType::PIN));

  managed_session_service()->OnUnlockScreenAttempt(
      true, session_manager::UnlockType::PASSWORD);

  ASSERT_EQ(ObservedUnlockFailureCount(), 0);
  EXPECT_FALSE(locked_);
  EXPECT_TRUE(unlocked_);
  EXPECT_THAT(unlock_type_, Eq(session_manager::UnlockType::PASSWORD));
}
}  // namespace policy
