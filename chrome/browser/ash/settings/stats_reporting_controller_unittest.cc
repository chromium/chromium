// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/stats_reporting_controller.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/device_settings_cache.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* kOwner = policy::PolicyBuilder::kFakeUsername;
constexpr char kNonOwner[] = "non_owner@example.com";

PrefService* local_state() {
  return TestingBrowserProcess::GetGlobal()->local_state();
}

}  // namespace

class StatsReportingControllerTest : public testing::Test {
 protected:
  StatsReportingControllerTest() = default;
  ~StatsReportingControllerTest() override = default;

  void SetUp() override {
    scoped_install_attributes_ =
        std::make_unique<ScopedStubInstallAttributes>();
    fake_session_manager_client_ = std::make_unique<FakeSessionManagerClient>();
    scoped_device_settings_ =
        std::make_unique<ScopedTestDeviceSettingsService>();

    cros_settings_holder_ = std::make_unique<CrosSettingsHolder>(
        ash::DeviceSettingsService::Get(), local_state());

    StatsReportingController::Initialize(local_state());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(local_state());

    device_policy_.Build();
    fake_session_manager_client_->set_device_policy(device_policy_.GetBlob());

    both_keys->ImportPrivateKeyAndSetPublicKey(*device_policy_.GetSigningKey());
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
  TestingProfile* CreateUser(const char* username,
                             scoped_refptr<ownership::MockOwnerKeyUtil> keys) {
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        keys);

    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(username, GaiaId(username));

    if (!user_manager::UserManager::Get()->FindUser(account_id)) {
      EXPECT_TRUE(user_session_manager_->AddRegularUser(account_id));
    }

    ash::ScopedAccountIdAnnotator annotator(profile_manager_->profile_manager(),
                                            account_id);

    TestingProfile* user = profile_manager_->CreateTestingProfile(username);

    // Initialize NSS for the user in case it tries to access or generate a
    // private key.
    FakeNssService::InitializeForBrowserContext(user,
                                                /*enable_system_slot=*/false);

    OwnerSettingsServiceAshFactory::GetForBrowserContext(user)
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

    user_session_manager_.reset();
    profile_manager_.reset();
    cros_settings_holder_.reset();
    scoped_device_settings_.reset();
    fake_session_manager_client_.reset();
    scoped_install_attributes_.reset();
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<ScopedStubInstallAttributes> scoped_install_attributes_;
  std::unique_ptr<FakeSessionManagerClient> fake_session_manager_client_;
  std::unique_ptr<ScopedTestDeviceSettingsService> scoped_device_settings_;
  std::unique_ptr<CrosSettingsHolder> cros_settings_holder_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ash::test::TestUserSessionManager> user_session_manager_;
  policy::DevicePolicyBuilder device_policy_;

  bool value_at_last_notification_{false};
  base::CallbackListSubscription observer_subscription_;

  scoped_refptr<ownership::MockOwnerKeyUtil> both_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> public_key_only{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  scoped_refptr<ownership::MockOwnerKeyUtil> no_keys{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
};

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipUnknown) {
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  TestingProfile* user = CreateUser(kNonOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(user, true);
  // A pending value is written in case there is no owner. It will be cleared
  // and written properly when ownership is taken. We will read from the
  // pending value before ownership is taken (pending value exists).
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  StatsReportingController::Get()->SetEnabled(user, false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIs(false);
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipNone) {
  DeviceSettingsService::Get()->StartProcessing(
      local_state(), fake_session_manager_client_.get(), no_keys);
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipNone,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // Before the device is owned, the value is written as a pending value:
  TestingProfile* user = CreateUser(kNonOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(user, true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  StatsReportingController::Get()->SetEnabled(user, false);
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIs(false);
  ExpectThatSignedStoredValueIs(false);
}

TEST_F(StatsReportingControllerTest, GetAndSet_OwnershipTaken) {
  DeviceSettingsService::Get()->StartProcessing(
      local_state(), fake_session_manager_client_.get(), both_keys);
  TestingProfile* owner = CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // When the device is owned, the owner can sign and store the value:
  StatsReportingController::Get()->SetEnabled(owner, true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);

  StatsReportingController::Get()->OnSignedPolicyStored(true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(true);

  StatsReportingController::Get()->SetEnabled(owner, false);
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
  DeviceSettingsService::Get()->StartProcessing(
      local_state(), fake_session_manager_client_.get(), both_keys);
  CreateUser(kOwner, both_keys);

  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());
  EXPECT_FALSE(StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(value_at_last_notification_);
  ExpectThatPendingValueIsNotSet();
  ExpectThatSignedStoredValueIs(false);

  // Setting value has no effect from a non-owner once device is owned:
  TestingProfile* non_owner = CreateUser(kNonOwner, public_key_only);
  StatsReportingController::Get()->SetEnabled(non_owner, true);
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
  TestingProfile* pre_ownership_user = CreateUser(kOwner, no_keys);
  StatsReportingController::Get()->SetEnabled(pre_ownership_user, true);
  EXPECT_TRUE(StatsReportingController::Get()->IsEnabled());
  EXPECT_TRUE(value_at_last_notification_);
  ExpectThatPendingValueIs(true);
  ExpectThatSignedStoredValueIs(false);

  DeviceSettingsService::Get()->StartProcessing(
      local_state(), fake_session_manager_client_.get(), both_keys);
  profile_manager_->DeleteTestingProfile(kOwner);
  TestingProfile* owner = CreateUser(kOwner, both_keys);
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            DeviceSettingsService::Get()->GetOwnershipStatus());

  // After device is owned, the value is written to Cros settings.
  StatsReportingController::Get()->OnOwnershipTaken(
      OwnerSettingsServiceAshFactory::GetForBrowserContext(owner));
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
