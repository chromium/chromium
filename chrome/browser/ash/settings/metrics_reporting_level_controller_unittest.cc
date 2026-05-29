// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/metrics_reporting_level_controller.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/device_settings_cache.h"
#include "components/metrics/metrics_reporting_level.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* kOwner = policy::PolicyBuilder::kFakeUsername;
constexpr char kNonOwner[] = "non_owner@example.com";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  MetricsReportingLevelController::RegisterLocalStatePrefs(
      local_state->registry());
  device_settings_cache::RegisterPrefs(local_state->registry());
  return local_state;
}

}  // namespace

class MetricsReportingLevelControllerTest : public testing::Test {
 protected:
  MetricsReportingLevelControllerTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~MetricsReportingLevelControllerTest() override = default;

  void SetUp() override {
    MetricsReportingLevelController::Initialize(&local_state_);

    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    both_keys->ImportPrivateKeyAndSetPublicKey(*device_policy_.GetSigningKey());
    public_key_only->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    // Prevent new keys from being generated.
    no_keys->SimulateGenerateKeyFailure(/*fail_times=*/999);

    observer_subscription_ =
        MetricsReportingLevelController::Get()->AddObserver(base::BindRepeating(
            &MetricsReportingLevelControllerTest::OnNotifiedOfChange,
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

  void ExpectThatPendingValueIs(metrics::MetricsReportingLevel expected) {
    std::optional<base::Value> pending =
        MetricsReportingLevelController::Get()->GetPendingValue();
    EXPECT_TRUE(pending.has_value());
    EXPECT_TRUE(pending->is_int());
    EXPECT_EQ(static_cast<int>(expected), pending->GetInt());
  }

  void ExpectThatPendingValueIsNotSet() {
    std::optional<base::Value> pending =
        MetricsReportingLevelController::Get()->GetPendingValue();
    EXPECT_FALSE(pending.has_value());
  }

  void ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel expected) {
    std::optional<base::Value> stored =
        MetricsReportingLevelController::Get()->GetSignedStoredValue();
    EXPECT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->is_int());
    EXPECT_EQ(static_cast<int>(expected), stored->GetInt());
  }

  void OnNotifiedOfChange() {
    level_at_last_notification_ =
        MetricsReportingLevelController::Get()->GetLevel();
  }

  void TearDown() override {
    observer_subscription_ = {};
    MetricsReportingLevelController::Shutdown();
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

  metrics::MetricsReportingLevel level_at_last_notification_{
      metrics::MetricsReportingLevel::kNone};
  base::CallbackListSubscription observer_subscription_;

  scoped_refptr<ownership::MockOwnerKeyUtil> both_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> public_key_only{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> no_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  user_manager::ScopedUserManager user_manager_enabler_;
};

TEST_F(MetricsReportingLevelControllerTest, GetAndSet_OwnershipUnknown) {
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  // The signed stored value should also be kNone because it's the default in
  // CrosSettings for kMetricsReportingLevelPref if not set.
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  std::unique_ptr<TestingProfile> user = CreateUser(kNonOwner, no_keys);
  MetricsReportingLevelController::Get()->SetLevel(
      user.get(), metrics::MetricsReportingLevel::kAdvanced);
  // A pending value is written in case there is no owner. It will be cleared
  // and written properly when ownership is taken. We will read from the
  // pending value before ownership is taken (pending value exists).
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kAdvanced);
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  MetricsReportingLevelController::Get()->SetLevel(
      user.get(), metrics::MetricsReportingLevel::kBasic);
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kBasic);
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);
}

TEST_F(MetricsReportingLevelControllerTest, GetAndSet_OwnershipNone) {
  DeviceSettingsService::Get()->StartProcessing(
      TestingBrowserProcess::GetGlobal()->local_state(),
      &fake_session_manager_client_, no_keys);
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipNone,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  // Before the device is owned, the value is written as a pending value:
  std::unique_ptr<TestingProfile> user = CreateUser(kNonOwner, no_keys);
  MetricsReportingLevelController::Get()->SetLevel(
      user.get(), metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kAdvanced);
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  MetricsReportingLevelController::Get()->SetLevel(
      user.get(), metrics::MetricsReportingLevel::kBasic);
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kBasic);
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);
}

TEST_F(MetricsReportingLevelControllerTest, GetAndSet_OwnershipTaken) {
  DeviceSettingsService::Get()->StartProcessing(
      TestingBrowserProcess::GetGlobal()->local_state(),
      &fake_session_manager_client_, both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  // When the device is owned, the owner can sign and store the value:
  MetricsReportingLevelController::Get()->SetLevel(
      owner.get(), metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kAdvanced);

  MetricsReportingLevelController::Get()->OnSignedPolicyStored(true);
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kAdvanced);

  MetricsReportingLevelController::Get()->SetLevel(
      owner.get(), metrics::MetricsReportingLevel::kBasic);
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kBasic);

  MetricsReportingLevelController::Get()->OnSignedPolicyStored(true);
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kBasic);
}

TEST_F(MetricsReportingLevelControllerTest, GetAndSet_OwnershipTaken_NonOwner) {
  DeviceSettingsService::Get()->StartProcessing(
      TestingBrowserProcess::GetGlobal()->local_state(),
      &fake_session_manager_client_, both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  // Setting value has no effect from a non-owner once device is owned:
  std::unique_ptr<TestingProfile> non_owner =
      CreateUser(kNonOwner, public_key_only);
  MetricsReportingLevelController::Get()->SetLevel(
      non_owner.get(), metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);
}

TEST_F(MetricsReportingLevelControllerTest, SetBeforeOwnershipTaken) {
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kNone, level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  // Before device is owned, setting the value means writing a pending value:
  std::unique_ptr<TestingProfile> pre_ownership_user =
      CreateUser(kOwner, no_keys);
  MetricsReportingLevelController::Get()->SetLevel(
      pre_ownership_user.get(), metrics::MetricsReportingLevel::kAdvanced);
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kAdvanced);
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kNone);

  DeviceSettingsService::Get()->StartProcessing(
      TestingBrowserProcess::GetGlobal()->local_state(),
      &fake_session_manager_client_, both_keys);
  std::unique_ptr<TestingProfile> owner = CreateUser(kOwner, both_keys);
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());

  // After device is owned, the value is written to Cros settings.
  MetricsReportingLevelController::Get()->OnOwnershipTaken(
      OwnerSettingsServiceAshFactory::GetForBrowserContext(owner.get()));
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIs(metrics::MetricsReportingLevel::kAdvanced);

  MetricsReportingLevelController::Get()->OnSignedPolicyStored(true);
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            MetricsReportingLevelController::Get()->GetLevel());
  EXPECT_EQ(metrics::MetricsReportingLevel::kAdvanced,
            level_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kAdvanced);
}

TEST_F(MetricsReportingLevelControllerTest, DefaultValue_EnterpriseManaged) {
  scoped_install_attributes_.Get()->SetCloudManaged("example.com", "id");
  DeviceSettingsService::Get()->StartProcessing(
      TestingBrowserProcess::GetGlobal()->local_state(),
      &fake_session_manager_client_, public_key_only);
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(metrics::MetricsReportingLevel::kBasic,
            MetricsReportingLevelController::Get()->GetLevel());
  ExpectThatSignedStoredValueIs(metrics::MetricsReportingLevel::kBasic);
}

}  // namespace ash
