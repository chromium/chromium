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

  void ResetStateForTesting() {
    per_user_state_manager_->ResetStateForTesting();
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
        account_id, false, user_manager::UserType::kRegular, profile_.get());
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

  user_manager::User* RegisterChildUser(const AccountId& account_id) {
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
        account_id, false, user_manager::UserType::kChild, profile_.get());
  }

  void LoginRegularUser(user_manager::User* user) {
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->LoginUser(user->GetAccountId());
  }

  void LoginGuestUser(user_manager::User* user) {
    test_user_manager_->set_current_user_ephemeral(true);
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->LoginUser(user->GetAccountId());
  }

  void LoginChildUser(user_manager::User* user) {
    test_user_manager_->set_current_user_child(true);
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->LoginUser(user->GetAccountId());
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

    // Set up user manager.
    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    test_user_manager_->Initialize();
    RunUntilIdle();

    per_user_state_manager_ = std::make_unique<TestPerUserStateManager>(
        test_user_manager_.get(), &pref_service_, storage_limits_,
        signing_key_);

    ash::StartupUtils::RegisterPrefs(pref_service_.registry());
    PerUserStateManagerChromeOS::RegisterPrefs(pref_service_.registry());
  }

  void TearDown() override {
    // Clean up user manager.
    test_user_manager_->Shutdown();
    test_user_manager_->Destroy();
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
       GuestWithNoDeviceOwnerCanEnableConsent) {
  GetPerUserStateManager()->SetIsManaged(false);
  GetPerUserStateManager()->SetIsDeviceOwned(false);

  // Guest user went through oobe.
  SetGuestOobeMetricsConsent(true);

  // Simulate ephemeral user login.
  user_manager::User* guest_user = RegisterGuestUser();
  LoginGuestUser(guest_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Consent set by guest during OOBE.
  EXPECT_TRUE(GetPerUserStateManager()
                  ->GetCurrentUserReportingConsentIfApplicable()
                  .has_value());

  // Ensure state has been reset.
  EXPECT_FALSE(
      GetLocalState()->GetBoolean(ash::prefs::kOobeGuestMetricsEnabled));

  // Check to ensure that metrics consent is stored in profile pref.
  EXPECT_TRUE(
      GetTestProfile()->GetPrefs()->GetBoolean(prefs::kMetricsUserConsent));

  // Log store uses ephemeral partition regardless of device owner consent.
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Guest user can always change consent.
  EXPECT_TRUE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(guest_user));
}

TEST_F(PerUserStateManagerChromeOSTest,
       OwnerConsentDisabledChildConsentToggleEnabled) {
  // Create device owner.
  const AccountId owner_account_id =
      AccountId::FromUserEmailGaiaId("owner@example.com", "1");
  auto* owner_user = RegisterUser(owner_account_id);
  test_user_manager_->SetOwnerId(owner_account_id);
  InitializeProfileState(/*user_id=*/"owner@example.com",
                         /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate user login.
  LoginRegularUser(owner_user);
  RunUntilIdle();

  auto* child_user = RegisterChildUser(
      AccountId::FromUserEmailGaiaId("child@example.com", "2"));
  InitializeProfileState(/*user_id=*/"child@example.com",
                         /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);

  // Simulate child user login.
  ResetStateForTesting();
  LoginChildUser(child_user);
  RunUntilIdle();

  // Child user reporting can be changed as it's the parental OOBE flow.
  EXPECT_TRUE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(child_user));
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .value());
}

TEST_F(PerUserStateManagerChromeOSTest,
       OwnerConsentEnabledChildConsentToggleEnabled) {
  // Create device owner.
  const AccountId owner_account_id =
      AccountId::FromUserEmailGaiaId("owner@example.com", "1");
  auto* owner_user = RegisterUser(owner_account_id);
  test_user_manager_->SetOwnerId(owner_account_id);
  InitializeProfileState(/*user_id=*/"owner@example.com",
                         /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate user login.
  LoginRegularUser(owner_user);
  RunUntilIdle();

  auto* child_user = RegisterChildUser(
      AccountId::FromUserEmailGaiaId("child@example.com", "2"));
  InitializeProfileState(/*user_id=*/"child@example.com",
                         /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);

  // Simulate child user login.
  ResetStateForTesting();
  LoginChildUser(child_user);
  RunUntilIdle();

  // Child user reporting can be changed as it's the parental OOBE flow.
  EXPECT_TRUE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(child_user));
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .value());
}

TEST_F(PerUserStateManagerChromeOSTest,
       SecondaryUserCanEnableConsentWithOwnerConsentDisabled) {
  // Create device owner.
  const AccountId owner_account_id =
      AccountId::FromUserEmailGaiaId("owner@example.com", "1");
  auto* owner_user = RegisterUser(owner_account_id);
  test_user_manager_->SetOwnerId(owner_account_id);
  InitializeProfileState(/*user_id=*/"owner@example.com",
                         /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate user login.
  LoginRegularUser(owner_user);
  RunUntilIdle();

  auto* secondary_user = RegisterUser(
      AccountId::FromUserEmailGaiaId("secondary@example.com", "2"));
  InitializeProfileState(/*user_id=*/"secondary@example.com",
                         /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/false);
  GetPerUserStateManager()->SetIsManaged(false);

  // Simulate secondary user login.
  ResetStateForTesting();
  LoginRegularUser(secondary_user);
  RunUntilIdle();

  // Secondary user reporting consent can be changed even with device owner
  // consent disabled.
  EXPECT_TRUE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(secondary_user));

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

  // Simulate user login.
  LoginRegularUser(test_user1);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  GetPerUserStateManager()->SetCurrentUserMetricsConsent(true);

  // User consent should be set to true.
  EXPECT_TRUE(GetPerUserStateManager()
                  ->GetCurrentUserReportingConsentIfApplicable()
                  .has_value());

  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Create secondary user.
  ResetStateForTesting();
  auto* test_user2 =
      RegisterUser(AccountId::FromUserEmailGaiaId("test2@example.com", "2"));
  InitializeProfileState(/*user_id=*/"test2@example.com",
                         /*metrics_consent=*/false,
                         /*has_consented_to_metrics=*/false);

  // Simulate user login.
  LoginRegularUser(test_user2);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // User consent should still be true since that's the value of the primary
  // user.
  EXPECT_TRUE(GetPerUserStateManager()
                  ->GetCurrentUserReportingConsentIfApplicable()
                  .has_value());
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Enable user2's metrics consent and disable it.
  GetTestProfile()->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent, true);
  GetTestProfile()->GetPrefs()->SetBoolean(prefs::kMetricsUserConsent, false);

  // User consent should still be true since that's the value of the primary
  // user.
  EXPECT_TRUE(GetPerUserStateManager()
                  ->GetCurrentUserReportingConsentIfApplicable()
                  .has_value());
}

TEST_F(PerUserStateManagerChromeOSTest,
       PerUserDisabledWhenOwnershipStatusUnknown) {
  // Ownership status of device is unknown.
  GetPerUserStateManager()->SetIsDeviceStatusKnown(false);

  // Simulate user login.
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test1@example.com", "1"));
  LoginRegularUser(test_user);

  // User log store is created async. Ensure the log store loading finishes.
  RunUntilIdle();

  // Per-user should not run if ownership status is unknown.
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .has_value());
  EXPECT_FALSE(GetPerUserStateManager()->is_log_store_set());
}

TEST_F(PerUserStateManagerChromeOSTest, PerUserDisabledForDeviceOwner) {
  // Create device owner.
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("test@example.com", "1");
  auto* owner_user = RegisterUser(account_id);
  test_user_manager_->SetOwnerId(account_id);

  // Simulate user login.
  LoginRegularUser(owner_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Owner should not have a consent.
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .has_value());

  // Log store uses ephemeral partition regardless of device owner consent.
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());

  // Device owner cannot change per user consent.
  EXPECT_FALSE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(owner_user));
}

TEST_F(PerUserStateManagerChromeOSTest,
       PerUserDisabledWhenOwnershipStatusNone) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test1@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);

  // Ownership status of device is none.
  GetPerUserStateManager()->SetIsDeviceOwned(false);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Per-user should not run if ownership status is none.
  // We assume this device is likely the owner if it's not a guest user.
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .has_value());
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
  EXPECT_FALSE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(test_user));
}

TEST_F(PerUserStateManagerChromeOSTest, PerUserEnabledWhenGuestOnNewDevice) {
  auto* guest_user = RegisterGuestUser();
  InitializeProfileState(/*user_id=*/guest_user->display_email(),
                         /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);

  // Ownership status of device is None.
  GetPerUserStateManager()->SetIsDeviceOwned(false);

  // Simulate user login.
  LoginRegularUser(guest_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Per-user should not run if ownership status is none.
  // We assume this device is likely the owner if it's not a guest user.
  EXPECT_TRUE(GetPerUserStateManager()
                  ->GetCurrentUserReportingConsentIfApplicable()
                  .has_value());
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
  EXPECT_TRUE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(guest_user));
}

TEST_F(PerUserStateManagerChromeOSTest, PerUserDisabledWhenDeviceIsManaged) {
  auto* test_user =
      RegisterUser(AccountId::FromUserEmailGaiaId("test1@example.com", "1"));
  InitializeProfileState(/*user_id=*/"", /*metrics_consent=*/true,
                         /*has_consented_to_metrics=*/true);

  // Ownership status of device is unknown.
  GetPerUserStateManager()->SetIsManaged(true);

  // Simulate user login.
  LoginRegularUser(test_user);

  // User log store is created async. Ensure that the log store loading
  // finishes.
  RunUntilIdle();

  // Per-user should not run if ownership status is unknown.
  EXPECT_FALSE(GetPerUserStateManager()
                   ->GetCurrentUserReportingConsentIfApplicable()
                   .has_value());
  EXPECT_TRUE(GetPerUserStateManager()->is_log_store_set());
  EXPECT_FALSE(
      GetPerUserStateManager()->IsUserAllowedToChangeConsent(test_user));
}

}  // namespace metrics
