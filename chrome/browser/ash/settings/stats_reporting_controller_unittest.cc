// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/stats_reporting_controller.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* kOwner = policy::PolicyBuilder::kFakeUsername;
constexpr char kNonOwner[] = "non_owner@example.com";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  StatsReportingController::RegisterLocalStatePrefs(local_state->registry());
  device_settings_cache::RegisterPrefs(local_state->registry());
  return local_state;
}

class StatsReportingControllerTest : public testing::Test {
 protected:
  StatsReportingControllerTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~StatsReportingControllerTest() override {}

  void SetUp() override {
    StatsReportingController::Initialize(&local_state_);

    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    both_keys->ImportPrivateKeyAndSetPublicKey(device_policy_.GetSigningKey());
    public_key_only->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    // Prevent new keys from being generated.
    no_keys->SimulateGenerateKeyFailure(/*fail_times=*/999);

    observer_subscription_ = StatsReportingController::Get()->AddObserver(
        base::BindRepeating(&StatsReportingControllerTest::OnNotifiedOfChange,
                            base::Unretained(this)));
  }

  // Creates and sets up a new profile. If `username` matches the username in
  // the device policies, the user will be recognized as the owner. `keys` will
  // be used to access / manipulate owner keys (note: access to the private
  // owner key is also a sign of being the owner).
  std::unique_ptr<TestingProfile> CreateUser(
      const char* username,
      scoped_refptr<ownership::MockOwnerKeyUtil> keys) {
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        keys);

    TestingProfile::Builder builder;
    builder.SetProfileName(username);
    std::unique_ptr<TestingProfile> user = builder.Build();

    // Initialize NSS for the user in case it tries to access or generate a
    // private key.
    FakeNssService::InitializeForBrowserContext(user.get(),
                                                /*enable_system_slot=*/false);

    OwnerSettingsServiceAshFactory::GetForBrowserContext(user.get())
        ->OnTPMTokenReady();
    content::RunAllTasksUntilIdle();
    return user;
  }

  void ExpectThatPendingValueIs(bool expected) {
    std::optional<base::Value> pending =
        StatsReportingController::Get()->GetPendingValue();
    EXPECT_TRUE(pending.has_value());
    EXPECT_TRUE(pending->is_bool());
    EXPECT_EQ(expected, pending->GetBool());
  }

  void ExpectThatPendingValueIsNotSet() {
    std::optional<base::Value> pending =
        StatsReportingController::Get()->GetPendingValue();
    EXPECT_FALSE(pending.has_value());
  }

  void ExpectThatSignedStoredValueIs(bool expected) {
    std::optional<base::Value> stored =
        StatsReportingController::Get()->GetSignedStoredValue();
    EXPECT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->is_bool());
    EXPECT_EQ(expected, stored->GetBool());
  }

  void OnNotifiedOfChange() {
    value_at_last_notification_ = StatsReportingController::Get()->IsEnabled();
  }

  void TearDown() override {
    observer_subscription_ = {};
    StatsReportingController::Shutdown();
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  TestingPrefServiceSimple local_state_;
  ScopedStubInstallAttributes scoped_install_attributes_;
  FakeSessionManagerClient fake_session_manager_client_;
  ScopedTestDeviceSettingsService scoped_device_settings_;
  CrosSettingsHolder cros_settings_holder_{ash::DeviceSettingsService::Get(),
                                           RegisterPrefs(&local_state_)};
  policy::DevicePolicyBuilder device_policy_;

  bool value_at_last_notification_{false};
  base::CallbackListSubscription observer_subscription_;

  scoped_refptr<ownership::MockOwnerKeyUtil> both_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> public_key_only{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> no_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  user_manager::ScopedUserManager user_manager_enabler_;
};

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipUnknown) {
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  std::unique_ptr<TestingProfile> user = CreateUser(kNonOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(user.get(), true);
  // A pending value is written in case there is no owner. It will be cleared
  // and written properly when ownership is taken. We will read from the
  // pending value before ownership is taken (pending value exists).
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  StatsReportingController::Get()->SetEnabled(user.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIs(false);
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipNone) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  no_keys);
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipNone,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // Before the device is owned, the value is written as a pending value:
  std::unique_ptr<TestingProfile> user = CreateUser(kNonOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(user.get(), true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  StatsReportingController::Get()->SetEnabled(user.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIs(false);
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipTaken) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // When the device is owned, the owner can sign and store the value:
  StatsReportingController::Get()->SetEnabled(owner.get(), true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);

  StatsReportingController::Get()->OnSignedPolicyStored(true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(true);

  StatsReportingController::Get()->SetEnabled(owner.get(), false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIs(false);

  StatsReportingController::Get()->OnSignedPolicyStored(true);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipTaken_NonOwner) {
  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // Setting value has no effect from a non-owner once device is owned:
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, public_key_only);
  StatsReportingController::Get()->SetEnabled(non_owner.get(), true);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, SetBeforeOwnershipTaken) {
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // Before device is owned, setting the value means writing a pending value:
  std::unique_ptr<TestingProfile> pre_ownership_user =
      CreateUser(kOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(pre_ownership_user.get(), true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  DeviceSettingsService::Get()->SetSessionManager(&fake_session_manager_client_,
                                                  both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());

  // After device is owned, the value is written to Cros settings.
  StatsReportingController::Get()->OnOwnershipTaken(
      OwnerSettingsServiceAshFactory::GetForBrowserContext(owner.get()));
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);

  StatsReportingController::Get()->OnSignedPolicyStored(true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(true);
}

}  // namespace ash
