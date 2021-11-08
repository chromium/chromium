// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"

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
      : PerUserStateManagerChromeOS(nullptr,
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

  bool is_log_store_set() const { return is_log_store_set_; }
  bool is_client_id_reset() const { return is_client_id_reset_; }
  bool is_metrics_reporting_enabled() const { return metrics_reporting_state_; }

 protected:
  void UnsetUserLogStore() override { is_log_store_set_ = false; }

  void ForceClientIdReset() override { is_client_id_reset_ = true; }

  bool IsReportingPolicyManaged() const override { return is_managed_; }

  void SetReportingState(bool metrics_consent) override {
    metrics_reporting_state_ = metrics_consent;
  }

  bool GetDeviceMetricsConsent() const override {
    return device_metrics_consent_;
  }

  bool HasUserLogStore() const override { return is_log_store_set_; }

 private:
  bool is_log_store_set_ = false;
  bool is_client_id_reset_ = false;
  bool metrics_reporting_state_ = true;
  bool is_managed_ = false;
  bool device_metrics_consent_ = true;
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
    PerUserStateManagerChromeOS::RegisterProfilePrefs(registry.get());
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
    PerUserStateManagerChromeOS::RegisterProfilePrefs(registry.get());
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
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  PrefService* GetLocalState() { return &pref_service_; }
  Profile* GetTestProfile() { return profile_.get(); }
  TestPerUserStateManager* GetPerUserStateManager() {
    return per_user_state_manager_.get();
  }

 protected:
  void SetUp() override {
    storage_limits_.min_ongoing_log_queue_count = 5;
    storage_limits_.min_ongoing_log_queue_size = 10000;
    storage_limits_.max_ongoing_log_size = 0;

    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();

    per_user_state_manager_ = std::make_unique<TestPerUserStateManager>(
        test_user_manager_.get(), &pref_service_, storage_limits_,
        signing_key_);

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

  // Ensure that reporting is disabled in the metrics service.
  EXPECT_FALSE(GetPerUserStateManager()->is_metrics_reporting_enabled());

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

  // Ensure that reporting is enabled in the metrics service.
  EXPECT_TRUE(GetPerUserStateManager()->is_metrics_reporting_enabled());

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

  // Ensure that reporting is enabled in the metrics service.
  EXPECT_TRUE(GetPerUserStateManager()->is_metrics_reporting_enabled());

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

}  // namespace metrics
