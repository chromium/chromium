// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeGaiaId[] = "1234567890";
}  // namespace

namespace policy {

class TPMAutoUpdateModePolicyHandlerTest : public testing::Test {
 public:
  TPMAutoUpdateModePolicyHandlerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_.get())) {
    ash::SessionManagerClient::InitializeFakeInMemory();
  }

  ~TPMAutoUpdateModePolicyHandlerTest() override {
    ash::SessionManagerClient::Shutdown();
  }

  void SetAutoUpdateMode(AutoUpdateMode auto_update_mode) {
    base::Value::Dict dict;
    dict.Set(ash::tpm_firmware_update::kSettingsKeyAutoUpdateMode,
             static_cast<int>(auto_update_mode));
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kTPMFirmwareUpdateSettings, base::Value(std::move(dict)));
    base::RunLoop().RunUntilIdle();
  }

  void CheckForUpdate(base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(update_available_);
  }

  void ShowNotification(ash::TpmAutoUpdateUserNotification notification_type) {
    last_shown_notification_ = notification_type;
  }

 protected:
  bool update_available_ = false;
  ash::TpmAutoUpdateUserNotification last_shown_notification_ =
      ash::TpmAutoUpdateUserNotification::kNone;

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  // Set up fake install attributes to pretend the machine is enrolled.
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("example.com", "fake-id")};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  base::WeakPtrFactory<TPMAutoUpdateModePolicyHandlerTest> weak_factory_{this};
};

// Verify if the TPM updates are triggered (or not) according to the device
// policy option TPMFirmwareUpdateSettings.AutoUpdateMode.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, PolicyUpdatesTriggered) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));

  update_available_ = true;

  auto* fake_session_manager_client = ash::FakeSessionManagerClient::Get();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0, fake_session_manager_client->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1, fake_session_manager_client->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kNever);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1, fake_session_manager_client->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      2, fake_session_manager_client->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kEnrollment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      2, fake_session_manager_client->start_tpm_firmware_update_call_count());
}

// Verify that the DBus call to start TPM firmware update is not triggered if
// state preserving update is not available.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, NoUpdatesAvailable) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));

  update_available_ = false;

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, ash::FakeSessionManagerClient::Get()
                   ->start_tpm_firmware_update_call_count());
}

// Verify that the notification informing the user that an update is planned
// after 24 hours is shown.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, ShowPlannedUpdateNotification) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));
  tpm_update_policy_handler.SetShowNotificationCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::ShowNotification,
                          base::Unretained(this)));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
  user_manager_->AddUser(account_id);
  user_manager_->LoginUser(account_id);

  update_available_ = true;

  EXPECT_EQ(last_shown_notification_,
            ash::TpmAutoUpdateUserNotification::kNone);

  SetAutoUpdateMode(AutoUpdateMode::kUserAcknowledgment);
  base::RunLoop().RunUntilIdle();

  // TPM update is not triggered.
  EXPECT_EQ(0, ash::FakeSessionManagerClient::Get()
                   ->start_tpm_firmware_update_call_count());

  EXPECT_EQ(last_shown_notification_,
            ash::TpmAutoUpdateUserNotification::kPlanned);
}

// Verify that the notification informing the user that an update will happen at
// the next reboot is shown.
TEST_F(TPMAutoUpdateModePolicyHandlerTest,
       ShowUpdateOnRebootNotificationNoTimer) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));
  tpm_update_policy_handler.SetShowNotificationCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::ShowNotification,
                          base::Unretained(this)));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
  user_manager_->AddUser(account_id);
  user_manager_->LoginUser(account_id);

  update_available_ = true;

  // First notification was shwed more than 24 hours ago.
  base::Time yesterday = base::Time::Now() - base::Hours(25);
  local_state_.Get()->SetInt64(
      prefs::kTPMUpdatePlannedNotificationShownTime,
      yesterday.ToDeltaSinceWindowsEpoch().InSeconds());

  SetAutoUpdateMode(AutoUpdateMode::kUserAcknowledgment);
  base::RunLoop().RunUntilIdle();

  // TPM update is not triggered.
  EXPECT_EQ(0, ash::FakeSessionManagerClient::Get()
                   ->start_tpm_firmware_update_call_count());

  // Show planned update notification.
  EXPECT_EQ(last_shown_notification_,
            ash::TpmAutoUpdateUserNotification::kOnNextReboot);
}

// Verify that the notification informing the user that an update will happen at
// the next reboot is triggered by the timer.
TEST_F(TPMAutoUpdateModePolicyHandlerTest,
       ShowUpdateOnRebootNotificationTimer) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));
  tpm_update_policy_handler.SetShowNotificationCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::ShowNotification,
                          base::Unretained(this)));

  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  auto* mock_timer_ptr = mock_timer.get();

  tpm_update_policy_handler.SetNotificationTimerForTesting(
      std::move(mock_timer));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
  user_manager_->AddUser(account_id);
  user_manager_->LoginUser(account_id);

  update_available_ = true;

  SetAutoUpdateMode(AutoUpdateMode::kUserAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, ash::FakeSessionManagerClient::Get()
                   ->start_tpm_firmware_update_call_count());

  // Show planned update notification.
  EXPECT_EQ(last_shown_notification_,
            ash::TpmAutoUpdateUserNotification::kPlanned);

  mock_timer_ptr->Fire();

  // Show update at reboot notification.
  EXPECT_EQ(last_shown_notification_,
            ash::TpmAutoUpdateUserNotification::kOnNextReboot);
}

// TPM update with user acknowlegment triggered.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, UpdateWithUserAcknowlegment) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      ash::CrosSettings::Get(), local_state_.Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));
  tpm_update_policy_handler.SetShowNotificationCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::ShowNotification,
                          base::Unretained(this)));

  update_available_ = true;

  // Update at next reboot notification already shown.
  local_state_.Get()->SetBoolean(prefs::kTPMUpdateOnNextRebootNotificationShown,
                                 true);
  SetAutoUpdateMode(AutoUpdateMode::kUserAcknowledgment);
  base::RunLoop().RunUntilIdle();

  // Update is triggered.
  EXPECT_EQ(1, ash::FakeSessionManagerClient::Get()
                   ->start_tpm_firmware_update_call_count());
}

}  // namespace policy
