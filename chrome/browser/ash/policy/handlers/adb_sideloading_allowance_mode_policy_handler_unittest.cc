// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/notifications/mock_adb_sideloading_policy_change_notification.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr GaiaId::Literal kFakeGaiaId("1234567890");

}  // namespace

namespace policy {

class AdbSideloadingAllowanceModePolicyHandlerTest : public testing::Test {
 public:
  using NotificationType = ash::AdbSideloadingPolicyChangeNotification::Type;

  AdbSideloadingAllowanceModePolicyHandlerTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_.get())),
        mock_notification_(new ash::MockAdbSideloadingPolicyChangeNotification(
            TestingBrowserProcess::GetGlobal()
                ->platform_part()
                ->browser_policy_connector_ash())) {
    chromeos::PowerManagerClient::InitializeFake();

    adb_sideloading_allowance_mode_policy_handler_ =
        std::make_unique<AdbSideloadingAllowanceModePolicyHandler>(
            ash::CrosSettings::Get(),
            TestingBrowserProcess::GetGlobal()->local_state(),
            chromeos::PowerManagerClient::Get(),
            // Ownership is moved to AdbSideloadingAllowanceModePolicyHandler.
            base::WrapUnique(mock_notification_.get()));

    adb_sideloading_allowance_mode_policy_handler_
        ->SetCheckSideloadingStatusCallbackForTesting(
            base::BindRepeating(&AdbSideloadingAllowanceModePolicyHandlerTest::
                                    CheckSideloadingStatus,
                                weak_factory_.GetWeakPtr()));
  }

  ~AdbSideloadingAllowanceModePolicyHandlerTest() override {
    adb_sideloading_allowance_mode_policy_handler_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  void CheckSideloadingStatus(base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(is_arc_sideloading_enabled_);
  }

 protected:
  void EnableSideloading() { is_arc_sideloading_enabled_ = true; }

  void DisableSideloading() { is_arc_sideloading_enabled_ = false; }

  void SetDevicePolicy(int policy) {
    scoped_testing_cros_settings_.device_settings()->SetInteger(
        ash::kDeviceCrostiniArcAdbSideloadingAllowed, policy);
    base::RunLoop().RunUntilIdle();
  }

  void SetDevicePolicyToAllow() {
    SetDevicePolicy(
        enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto::
            ALLOW_FOR_AFFILIATED_USERS);
  }

  void SetDevicePolicyToDisallow() {
    SetDevicePolicy(enterprise_management::
                        DeviceCrostiniArcAdbSideloadingAllowedProto::DISALLOW);
  }

  void SetDevicePolicyToDisallowWithPowerwash() {
    SetDevicePolicy(
        enterprise_management::DeviceCrostiniArcAdbSideloadingAllowedProto::
            DISALLOW_WITH_POWERWASH);
  }

  void CreateUser() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
  }

  void FakePlannedNotificationTime() {
    // Fake that the first notification had been displayed more than 24 hours
    // ago.
    base::Time yesterday = base::Time::Now() - base::Hours(25);
    TestingBrowserProcess::GetGlobal()->local_state()->SetTime(
        prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime, yesterday);
  }

  bool is_arc_sideloading_enabled_ = false;

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  // Needed for `browser_policy_connector_ash()`.
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  raw_ptr<ash::MockAdbSideloadingPolicyChangeNotification, DanglingUntriaged>
      mock_notification_;
  std::unique_ptr<AdbSideloadingAllowanceModePolicyHandler>
      adb_sideloading_allowance_mode_policy_handler_;

  base::WeakPtrFactory<AdbSideloadingAllowanceModePolicyHandlerTest>
      weak_factory_{this};
};

// Verify that when the device policy is set to DISALLOW, but
// arc_sideloading_allowed is false that no notification is displayed
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowNotificationDisabled) {
  CreateUser();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);

  DisableSideloading();
  SetDevicePolicyToDisallow();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);
}

// Verify that when the device policy is set to DISALLOW, the notification is
// displayed
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowNotificationEnabled) {
  CreateUser();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);

  EnableSideloading();
  SetDevicePolicyToDisallow();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kSideloadingDisallowed);
}

// Verify that when the device policy is set to DISALLOW_WITH_POWERWASH, but
// arc_sideloading_allowed is false that no notification is displayed
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowWithPowerwashNotificationDisabled) {
  CreateUser();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);

  DisableSideloading();
  SetDevicePolicyToDisallowWithPowerwash();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);
}

// Verify that when the device policy is set to DISALLOW_WITH_POWERWASH, the
// first notification is displayed
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowWithPowerwashNotificationEnabled) {
  CreateUser();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kNone);

  EnableSideloading();
  SetDevicePolicyToDisallowWithPowerwash();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kPowerwashPlanned);
}

// Verify that the second powerwash notification is displayed when the time runs
// out
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowWithPowerwashNotificationTimeRunsOut) {
  CreateUser();
  EnableSideloading();
  FakePlannedNotificationTime();
  SetDevicePolicyToDisallowWithPowerwash();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kPowerwashOnNextReboot);
}

// Verify that the second powerwash notification is displayed when the timer is
// triggered
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest,
       ShowDisallowWithPowerwashNotificationTimerTrigger) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  auto* mock_timer_ptr = mock_timer.get();
  adb_sideloading_allowance_mode_policy_handler_
      ->SetNotificationTimerForTesting(std::move(mock_timer));

  CreateUser();
  EnableSideloading();
  SetDevicePolicyToDisallowWithPowerwash();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kPowerwashPlanned);

  mock_timer_ptr->Fire();

  EXPECT_EQ(mock_notification_->last_shown_notification,
            NotificationType::kPowerwashOnNextReboot);
}

// Verify that the preferences are reset correctly
TEST_F(AdbSideloadingAllowanceModePolicyHandlerTest, Preferences) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  auto* mock_timer_ptr = mock_timer.get();
  adb_sideloading_allowance_mode_policy_handler_
      ->SetNotificationTimerForTesting(std::move(mock_timer));

  // First all the preferences should have their default values
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown));
  EXPECT_EQ(TestingBrowserProcess::GetGlobal()->local_state()->GetTime(
                prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime),
            base::Time::Min());
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown));

  EnableSideloading();

  SetDevicePolicyToDisallow();
  base::RunLoop().RunUntilIdle();

  // Check that the preference for this notification is set
  EXPECT_TRUE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown));

  SetDevicePolicyToDisallowWithPowerwash();
  base::RunLoop().RunUntilIdle();

  // Check that the other notification's preference is reset
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown));
  // Check that the preferences for this notification are set
  EXPECT_NE(TestingBrowserProcess::GetGlobal()->local_state()->GetTime(
                prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime),
            base::Time::Min());
  mock_timer_ptr->Fire();
  EXPECT_TRUE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown));

  SetDevicePolicyToDisallow();
  base::RunLoop().RunUntilIdle();

  // Check that the preference for this notification is set
  EXPECT_TRUE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown));
  // Check that the other notification's preferences are reset
  EXPECT_EQ(TestingBrowserProcess::GetGlobal()->local_state()->GetTime(
                prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime),
            base::Time::Min());
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown));

  SetDevicePolicyToAllow();
  base::RunLoop().RunUntilIdle();

  // Check that all the preferences are reset again
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingDisallowedNotificationShown));
  EXPECT_EQ(TestingBrowserProcess::GetGlobal()->local_state()->GetTime(
                prefs::kAdbSideloadingPowerwashPlannedNotificationShownTime),
            base::Time::Min());
  EXPECT_FALSE(TestingBrowserProcess::GetGlobal()->local_state()->GetBoolean(
      prefs::kAdbSideloadingPowerwashOnNextRebootNotificationShown));
}

}  // namespace policy
