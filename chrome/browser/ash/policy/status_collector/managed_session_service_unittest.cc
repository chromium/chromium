// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

using testing::Eq;

namespace policy {

namespace {

constexpr AccountId::Literal kAffiliatedAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user0@managed.com",
                                            GaiaId::Literal("1000"));
constexpr AccountId::Literal kUnaffiliatedAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user2@managed.com",
                                            GaiaId::Literal("1002"));
constexpr AccountId::Literal kSecondaryAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user3@managed.com",
                                            GaiaId::Literal("1003"));

}  // namespace

class ManagedSessionServiceTest : public ::testing::Test,
                                  public ManagedSessionService::Observer {
 protected:
  using SessionState = session_manager::SessionState;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ::ash::SessionManagerClient::InitializeFake();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    session_termination_manager_ =
        std::make_unique<::ash::SessionTerminationManager>();
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    auto delegate =
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process);
    session_delegate_ = delegate.get();
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            browser_process->local_state(), std::move(delegate));
    user_session_manager_ = std::make_unique<ash::UserSessionManager>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->application_locale_storage(),
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->browser_policy_connector_ash(),
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->component_manager_ash());

    managed_session_service_ =
        std::make_unique<ManagedSessionService>(&test_clock_);
  }

  void TearDown() override {
    managed_session_service_.reset();
    user_session_manager_->Shutdown();
    user_session_manager_.reset();
    logged_in_ = nullptr;
    logged_out_ = nullptr;
    session_delegate_ = nullptr;
    user_session_test_environment_.reset();
    session_termination_manager_.reset();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    ::ash::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  Profile& GuestLogin() {
    CHECK(user_session_test_environment_->AddGuestUser());
    user_session_test_environment_->LogIn(user_manager::GuestAccountId());
    return GetProfile(user_manager::GuestAccountId());
  }

  // Logs in `account_id`, which must have been added, and returns its
  // profile.
  Profile& LogIn(const AccountId& account_id, bool is_affiliated) {
    user_session_test_environment_->LogIn(account_id);
    // LogIn() resets the policy status.
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        account_id, /*is_managed=*/is_affiliated, is_affiliated);
    return GetProfile(account_id);
  }

  static Profile& GetProfile(const AccountId& account_id) {
    return CHECK_DEREF(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id)));
  }

  ash::test::UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

  ash::test::ChromeUserSessionTestEnvironmentDelegate& session_delegate() {
    return *session_delegate_;
  }

  ManagedSessionService* managed_session_service() {
    return managed_session_service_.get();
  }

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
  // NOTE: InstallAttributes is required to construct BrowserPolicyConnectorAsh.
  // CrosSettings is needed because otherwise TestingProfile automatically
  // creates ScopedCrosSettingsTestHelper, which conflicts with
  // ScopedStubInstallAttributes.
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<::ash::SessionTerminationManager>
      session_termination_manager_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<ash::test::ChromeUserSessionTestEnvironmentDelegate>
      session_delegate_ = nullptr;
  std::unique_ptr<ash::UserSessionManager> user_session_manager_;

  base::SimpleTestClock test_clock_;

  std::unique_ptr<ManagedSessionService> managed_session_service_;

  int observed_login_count_ = 0;

  int observed_session_termination_count_ = 0;

  int observed_kiosk_login_failure_count_ = 0;

  int observed_unlock_failure_count_ = 0;
};

TEST_F(ManagedSessionServiceTest, OnSessionStateChanged) {
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);
  session_manager::SessionManager::Get()->SetSessionState(SessionState::ACTIVE);

  EXPECT_TRUE(locked_);
  EXPECT_TRUE(unlocked_);

  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);

  EXPECT_TRUE(locked_);

  locked_ = false;
  unlocked_ = false;

  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);
  session_manager::SessionManager::Get()->SetSessionState(
      SessionState::LOGIN_PRIMARY);

  EXPECT_FALSE(locked_);
  EXPECT_TRUE(unlocked_);

  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);

  EXPECT_TRUE(locked_);

  locked_ = false;
  unlocked_ = false;

  session_manager::SessionManager::Get()->SetSessionState(SessionState::ACTIVE);

  EXPECT_TRUE(unlocked_);
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedAffiliatedAndPrimary) {
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kAffiliatedAccountId));
  Profile& affiliated_profile =
      LogIn(kAffiliatedAccountId, /*is_affiliated=*/true);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kAffiliatedAccountId);

  EXPECT_TRUE(affiliated_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedAffiliated) {
  // Profiles only exist for logged-in users, so log in an unaffiliated primary
  // user first: that keeps the affiliated user secondary, which is what this
  // test is about.
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kUnaffiliatedAccountId));
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kSecondaryAccountId));
  LogIn(kUnaffiliatedAccountId, /*is_affiliated=*/false);
  Profile& secondary_profile =
      LogIn(kSecondaryAccountId, /*is_affiliated=*/true);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kSecondaryAccountId);

  EXPECT_TRUE(secondary_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedPrimary) {
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kUnaffiliatedAccountId));
  Profile& unaffiliated_profile =
      LogIn(kUnaffiliatedAccountId, /*is_affiliated=*/false);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kUnaffiliatedAccountId);

  EXPECT_TRUE(unaffiliated_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnUserProfileLoadedGuest) {
  Profile& guest_profile = GuestLogin();
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      user_manager::GuestAccountId());

  EXPECT_TRUE(guest_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest,
       OnProfileWillBeDestroyedAffiliatedAndPrimary) {
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kAffiliatedAccountId));
  Profile& affiliated_profile =
      LogIn(kAffiliatedAccountId, /*is_affiliated=*/true);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kAffiliatedAccountId);
  affiliated_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(affiliated_profile.IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedAffiliated) {
  // Profiles only exist for logged-in users, so log in an unaffiliated primary
  // user first: that keeps the affiliated user secondary, which is what this
  // test is about.
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kUnaffiliatedAccountId));
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kSecondaryAccountId));
  LogIn(kUnaffiliatedAccountId, /*is_affiliated=*/false);
  Profile& secondary_profile =
      LogIn(kSecondaryAccountId, /*is_affiliated=*/true);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kSecondaryAccountId);
  secondary_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(secondary_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedPrimary) {
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kUnaffiliatedAccountId));
  Profile& unaffiliated_profile =
      LogIn(kUnaffiliatedAccountId, /*is_affiliated=*/false);
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      kUnaffiliatedAccountId);
  unaffiliated_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(unaffiliated_profile.IsSameOrParent(logged_in_));
}

TEST_F(ManagedSessionServiceTest, OnProfileWillBeDestroyedGuest) {
  Profile& guest_profile = GuestLogin();
  managed_session_service()->AddObserver(this);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      user_manager::GuestAccountId());
  guest_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(guest_profile.IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, SuspendDone) {
  managed_session_service()->AddObserver(this);
  test_clock()->SetNow(base::Time::Now());
  base::TimeDelta sleep_duration = base::Hours(2);

  power_manager_client()->SendSuspendDone(sleep_duration);

  EXPECT_EQ(*suspend_time_, test_clock()->Now() - sleep_duration);
}

TEST_F(ManagedSessionServiceTest, RemoveObserver) {
  const AccountId account_id = kAffiliatedAccountId;
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(account_id));
  Profile& profile = LogIn(account_id, /*is_affiliated=*/true);
  managed_session_service()->AddObserver(this);

  managed_session_service()->RemoveObserver(this);

  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);
  session_manager::SessionManager::Get()->SetSessionState(SessionState::ACTIVE);
  session_manager::SessionManager::Get()->SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(locked_);
  EXPECT_FALSE(unlocked_);

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(account_id);
  EXPECT_FALSE(profile.IsSameOrParent(logged_in_));

  profile.MaybeSendDestroyedNotification();
  EXPECT_FALSE(profile.IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, LoginFailure) {
  managed_session_service()->AddObserver(this);

  managed_session_service()->OnAuthFailure(
      ash::AuthFailure(ash::AuthFailure::FailureReason::OWNER_REQUIRED));

  EXPECT_EQ(auth_failure_.reason(),
            ash::AuthFailure::FailureReason::OWNER_REQUIRED);
}

TEST_F(ManagedSessionServiceTest, LoginBeforeCreate) {
  const AccountId affiliated_account_id = kAffiliatedAccountId;
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(affiliated_account_id));
  Profile& affiliated_profile =
      LogIn(affiliated_account_id, /*is_affiliated=*/true);

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  ASSERT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(affiliated_profile.IsSameOrParent(logged_in_));

  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(
      affiliated_account_id);

  EXPECT_EQ(ObservedLoginCount(), 1);

  affiliated_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(affiliated_profile.IsSameOrParent(logged_out_));
  ASSERT_EQ(ObservedSessionTerminationCount(), 0);

  ::ash::SessionTerminationManager::Get()->StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);

  EXPECT_EQ(ObservedSessionTerminationCount(), 1);
}

TEST_F(ManagedSessionServiceTest, GuestLoginBeforeCreate) {
  Profile& guest_profile = GuestLogin();

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  EXPECT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(guest_profile.IsSameOrParent(logged_in_));

  ::ash::SessionTerminationManager::Get()->StopSession(
      login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);

  EXPECT_EQ(ObservedSessionTerminationCount(), 1);

  guest_profile.MaybeSendDestroyedNotification();

  EXPECT_TRUE(guest_profile.IsSameOrParent(logged_out_));
}

TEST_F(ManagedSessionServiceTest, LoggedInProfileNotCreated) {
  // Start the session without creating the profile, which
  // UserSessionTestEnvironment::LogIn() does in the same step.
  const AccountId account_id = kAffiliatedAccountId;
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(account_id));
  session_manager::SessionManager::Get()->CreateSession(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
      /*new_user=*/false, /*has_active_session=*/false);
  session_manager::SessionManager::Get()->SessionStarted();

  ManagedSessionService managed_session_service;
  managed_session_service.AddObserver(this);

  EXPECT_EQ(ObservedLoginCount(), 0);

  // Simulate user profile loaded.
  {
    TestingProfileManager& profile_manager =
        session_delegate().testing_profile_manager();
    ash::ScopedAccountIdAnnotator annotator(profile_manager.profile_manager(),
                                            account_id);
    profile_manager.CreateTestingProfile(account_id.GetUserEmail());
  }
  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(account_id);

  ASSERT_EQ(ObservedLoginCount(), 1);
  EXPECT_TRUE(GetProfile(account_id).IsSameOrParent(logged_in_));
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
