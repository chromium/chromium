// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"

#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/unsent_log_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using ::testing::Eq;
using ::testing::Ne;

// Testing version of PerUserStateManager that decouples external dependencies
// used for unit testing purposes.
class TestPerUserStateManager : public PerUserStateManagerChromeOS {
 public:
  TestPerUserStateManager(user_manager::UserManager* user_manager,
                          PrefService* local_state,
                          const MetricsLogStore::StorageLimits& storage_limits,
                          const std::string& signing_key)
      : PerUserStateManagerChromeOS(/*metrics_service_client=*/nullptr,
                                    user_manager,
                                    local_state,
                                    storage_limits,
                                    signing_key) {}
  ~TestPerUserStateManager() override = default;

  void SetUserLogStore(std::unique_ptr<UnsentLogStore> log_store) override {
    is_log_store_set_ = true;
  }
  void SetIsManaged(bool is_managed) { is_managed_ = is_managed; }

  void SetDeviceMetricsConsent(bool metrics_consent) {
    device_metrics_consent_ = metrics_consent;
  }

  void SetIsDeviceOwned(bool is_device_owned) {
    is_device_owned_ = is_device_owned;
  }

  void SetIsDeviceStatusKnown(bool is_device_status_known) {
    is_device_status_known_ = is_device_status_known;
  }

  bool is_log_store_set() const { return is_log_store_set_; }
  bool is_client_id_reset() const { return is_client_id_reset_; }

 protected:
  void UnsetUserLogStore() override { is_log_store_set_ = false; }

  void ForceClientIdReset() override { is_client_id_reset_ = true; }

  bool IsReportingPolicyManaged() const override { return is_managed_; }

  bool GetDeviceMetricsConsent() const override {
    return device_metrics_consent_;
  }

  bool HasUserLogStore() const override { return is_log_store_set_; }

  bool IsDeviceOwned() const override { return is_device_owned_; }

  bool IsDeviceStatusKnown() const override { return is_device_status_known_; }

  void WaitForOwnershipStatus() override {
    if (IsDeviceStatusKnown()) {
      InitializeProfileMetricsState(
          is_device_owned_
              ? ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken
              : ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone);
    }
  }

 private:
  bool is_log_store_set_ = false;
  bool is_client_id_reset_ = false;
  bool is_managed_ = false;
  bool device_metrics_consent_ = true;
  bool is_device_owned_ = true;
  bool is_device_status_known_ = true;
};

}  // namespace

class PerUserStateManagerChromeOSTest : public testing::Test {
 public:
  PerUserStateManagerChromeOSTest() : signing_key_("signing_key") {}
  // Profiles must be destructed on the UI thread.
  ~PerUserStateManagerChromeOSTest() override {
    // Destruct before user manager because of dependency.
    per_user_state_manager_.reset();

    // Profiles must be destructed on the UI thread.
    profile_.reset();
  }

  user_manager::User* RegisterUser(const AccountId& account_id) {
    // Create profile.
    TestingProfile::Builder profile_builder;
    sync_preferences::PrefServiceMockFactory factory;
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

    RegisterUserProfilePrefs(registry.get());

    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    profile_builder.SetPrefService(std::move(prefs));
    profile_ = profile_builder.Build();

    return test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false, user_manager::USER_TYPE_REGULAR, profile_.get());
  }

  user_manager::User* RegisterGuestUser() {
    // Create profile.
    TestingProfile::Builder profile_builder;
    sync_preferences::PrefServiceMockFactory factory;
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

    RegisterUserProfilePrefs(registry.get());

    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    profile_builder.SetPrefService(std::move(prefs));
    profile_ = profile_builder.Build();

    auto* user = test_user_manager_->AddGuestUser();
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());
    return user;
  }

  void LoginRegularUser(user_manager::User* user) {
    test_user_manager_->LoginUser(user->GetAccountId());
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->SimulateUserProfileLoad(user->GetAccountId());
  }

  void LoginGuestUser(user_manager::User* user) {
    test_user_manager_->LoginUser(user->GetAccountId());
    test_user_manager_->set_current_user_ephemeral(true);
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->SimulateUserProfileLoad(user->GetAccountId());
  }

  void InitializeProfileState(const std::string& user_id,
                              bool metrics_consent,
                              bool has_consented_to_metrics) {
    profile_->GetPrefs()->SetString(prefs::kMetricsUserId, user_id);
    profile_->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent,
                                     metrics_consent);
    profile_->GetPrefs()->SetBoolean(
        prefs::kMetricsRequiresClientIdResetOnConsent,
        has_consented_to_metrics);
    profile_->GetPrefs()->SetBoolean(prefs::kMetricsUserInheritOwnerConsent,
                                     false);
  }

  void SetShouldInheritOwnerConsent(bool should_inherit) {
    profile_->GetPrefs()->SetBoolean(prefs::kMetricsUserInheritOwnerConsent,
                                     should_inherit);
  }

  void SetGuestOobeMetricsConsent(bool metrics_consent) {
    GetLocalState()->SetBoolean(ash::prefs::kOobeGuestMetricsEnabled,
                                metrics_consent);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  PrefService* GetLocalState() { return &pref_service_; }
  Profile* GetTestProfile() { return profile_.get(); }
  TestPerUserStateManager* GetPerUserStateManager() {
    return per_user_state_manager_.get();
  }

 protected:
  void SetUp() override {
    // Limits to ensure at least some logs will be persisted for the tests.
    storage_limits_ = {
        // Log store that can hold up to 5 logs. Set so that logs are not
        // dropped in the tests.
        .initial_log_queue_limits =
            UnsentLogStore::UnsentLogStoreLimits{
                .min_log_count = 5,
            },
        // Log store that can hold up to 5 logs. Set so that logs are not
        // dropped in the tests.
        .ongoing_log_queue_limits =
            UnsentLogStore::UnsentLogStoreLimits{
                .min_log_count = 5,
            },
    };

    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();

    per_user_state_manager_ = std::make_unique<TestPerUserStateManager>(
        test_user_manager_.get(), &pref_service_, storage_limits_,
        signing_key_);

    ash::StartupUtils::RegisterPrefs(pref_service_.registry());
    PerUserStateManagerChromeOS::RegisterPrefs(pref_service_.registry());
  }

  std::unique_ptr<TestPerUserStateManager> per_user_state_manager_;
  std::unique_ptr<ash::FakeChromeUserManager> test_user_manager_;
  std::unique_ptr<TestingProfile> profile_;

  TestingPrefServiceSimple pref_service_;

  // Profiles must be created in browser threads.
  content::BrowserTaskEnvironment task_environment_;

  MetricsLogStore::StorageLimits storage_limits_;
  std::string signing_key_;
};

TEST_F(PerUserStateManagerChromeOSTest, UserIdErasedWhenConsentTurnedOff) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test@example.com", "1"));
  InitializeProfileState(/*user_id=*/"user_id",
                         /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);
  GetPerUserStateManager()->SetIsManaged(false);

  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  GetPerUserStateManager()->SetCurrentUserMetricsConsent(false);

  EXPECT_FALSE(
      GetTestProfile()->GetPrefs()->GetBoolean(prefs::kMetricsUserConsent));
  EXPECT_TRUE(GetTestProfile()->GetPrefs()->GetBoolean(
      prefs::kMetricsRequiresClientIdResetOnConsent));

  // Client ID should only be reset when going from off->on.
  EXPECT_FALSE(GetPerUserStateManager()->is_client_id_reset());

  // Ensure that state is clean.
  EXPECT_THAT(GetTestProfile()->GetPrefs()->GetString(prefs::kMetricsUserId),
              Eq(""));
  EXPECT_THAT(GetLocalState()->GetString(prefs::kMetricsCurrentUserId), Eq(""));
}

TEST_F(PerUserStateManagerChromeOSTest,
       ClientIdReset_WhenConsentTurnedOn_AndUserSentMetrics) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/true);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  GetPerUserStateManager()->SetCurrentUserMetricsConsent(true);

  EXPECT_TRUE(
      GetTestProfile()->GetPrefs()->GetBoolean(prefs::kMetricsUserConsent));
  EXPECT_TRUE(GetTestProfile()->GetPrefs()->GetBoolean(
      prefs::kMetricsRequiresClientIdResetOnConsent));

  // Client ID should be reset when going from off->on and user has sent
  // metrics.
  EXPECT_TRUE(GetPerUserStateManager()->is_client_id_reset());
}

TEST_F(PerUserStateManagerChromeOSTest,
       ClientIdNotResetForUserWhoNeverReportedMetrics) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  GetPerUserStateManager()->SetCurrentUserMetricsConsent(true);

  EXPECT_TRUE(
      GetTestProfile()->GetPrefs()->GetBoolean(prefs::kMetricsUserConsent));
  EXPECT_TRUE(GetTestProfile()->GetPrefs()->GetBoolean(
      prefs::kMetricsRequiresClientIdResetOnConsent));

  // Client ID should not be reset when going from off->on and user had not sent
  // metrics.
  EXPECT_FALSE(GetPerUserStateManager()->is_client_id_reset());
}

TEST_F(PerUserStateManagerChromeOSTest,
       EphemeralLogStoreUsedForGuestSessionWithDisabledPolicy) {
  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetDeviceMetricsConsent(false);

  // Simulate ephemeral user login.
  LoginGuestUser(RegisterGuestUser());

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Log store should be set to use temporary cryptohome for when device metrics
  // consent is off.
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
}

TEST_F(PerUserStateManagerChromeOSTest,
       LocalStateLogStoreUsedForGuestWithEnabledPolicy) {
  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetDeviceMetricsConsent(true);

  // Simulate ephemeral user login.
  LoginGuestUser(RegisterGuestUser());

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Log store should not be loaded yet to store logs in local state for when
  // device metrics consent is on.
  EXPECT_FALSE(GetPerUserStateManager()->is_log_store_set());
}

TEST_F(PerUserStateManagerChromeOSTest,
       GuestWithNoDeviceOwnerLoadsConsentSetOnOobe) {
  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetIsDeviceOwned(false);

  // Guest user went through oobe.
  SetGuestOobeMetricsConsent(true);

  // Simulate ephemeral user login.
  LoginGuestUser(RegisterGuestUser());

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Consent set by guest during OOBE.
  EXPECT_TRUE(
      *GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());

  // Ensure state has been reset.
  EXPECT_FALSE(
      GetLocalState()->GetBoolean(ash::prefs::kOobeGuestMetricsEnabled));

  // Check to ensure that metrics consent is stored in profile pref.
  EXPECT_TRUE(
      GetTestProfile()->GetPrefs()->GetBoolean(prefs::kMetricsUserConsent));

  // Log store should be set to use ephemeral partition in the absence of a
  // device owner.
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
}

TEST_F(PerUserStateManagerChromeOSTest, OwnerCannotUsePerUser) {
  // Create device owner.
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("test@example.com", "1");
  auto* test_user = RegisterUser(account_id);
  test_user_manager_->SetOwnerId(account_id);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Owner should not have a consent.
  EXPECT_FALSE(
      GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());

  // User logs should still be persisted in the owner's cryptohome.
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
}

TEST_F(PerUserStateManagerChromeOSTest, NewUserInheritsOwnerConsent) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);

  // User should inherit owner consent if new user.
  SetShouldInheritOwnerConsent(true);

  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetDeviceMetricsConsent(true);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // User consent should be set to true since pref is true and device metrics
  // consent is also true.
  EXPECT_TRUE(
      GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());

  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
}

// Multi-user sessions are deprecated, but still need to be supported. This test
// ensures that the primary user (user originally used to login) is used and all
// other users are ignored.
TEST_F(PerUserStateManagerChromeOSTest, MultiUserUsesPrimaryUser) {
  auto* test_user1 =
      RegisterUser(AccountId::FromUserEmailGaiaId("test1@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/true);
  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetDeviceMetricsConsent(true);

  // Simulate user login.
  LoginRegularUser(test_user1);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  GetPerUserStateManager()->SetCurrentUserMetricsConsent(true);

  // User consent should be set to true since pref is true and device metrics
  // consent is also true.
  EXPECT_TRUE(
      *GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());

  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Create secondary user.
  TestingProfile::Builder profile_builder;
  sync_preferences::PrefServiceMockFactory factory;
  auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  RegisterUserProfilePrefs(registry.get());
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
      factory.CreateSyncable(registry.get()));
  profile_builder.SetPrefService(std::move(prefs));
  auto test_user2_profile = profile_builder.Build();
  AccountId test_user2_account_id =
      AccountId::FromUserEmailGaiaId("test2@example.com", "2");

  // Add user.
  user_manager::User* test_user2 =
      test_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          test_user2_account_id, false, user_manager::USER_TYPE_REGULAR,
          test_user2_profile.get());

  // Explicitly set the user consent to false.
  test_user2_profile->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent, false);

  // Simulate user login.
  LoginRegularUser(test_user2);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // User consent should still be true since that's the value of the primary
  // user.
  EXPECT_TRUE(
      *GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Enable user2's metrics consent and disable it.
  test_user2_profile->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent, true);
  test_user2_profile->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent, false);

  // User consent should still be true since that's the value of the primary
  // user.
  EXPECT_TRUE(
      *GetPerUserStateManager()->GetCurrentUserReportingConsentIfApplicable());

  // Profiles must be destructed on the UI thread.
  test_user2_profile.reset();
}

TEST_F(PerUserStateManagerChromeOSTest,
       PerUserDisabledWhenOwnershipStatusUnknown) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test1@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);

  // Ownership status of device is unknown.
  GetPerUserStateManager()->SetIsDeviceStatusKnown(false);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Per-user should not run if ownership status is unknown.
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .has_value());
  EXPECT_FALSE(GetPerUserStateManager()->is_log_store_set());
}

}  // namespace metrics
