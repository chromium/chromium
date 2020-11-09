// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_test_helpers.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Mock;

using MinimumVersionRequirement =
    policy::MinimumVersionPolicyHandler::MinimumVersionRequirement;

namespace chromeos {
namespace {

const char kFakeCurrentVersion[] = "13305.20.0";
const char kNewVersion[] = "13305.25.0";
const char kUpdateRequiredNotificationId[] = "policy.update_required";
const char kCellularServicePath[] = "/service/cellular1";
const int kLongWarningInDays = 10;
const int kShortWarningInDays = 2;

}  // namespace

class UpdateRequiredNotificationTest
    : public testing::Test,
      public policy::MinimumVersionPolicyHandler::Delegate {
 public:
  UpdateRequiredNotificationTest();

  void SetUp() override;
  void TearDown() override;

  // MinimumVersionPolicyHandler::Delegate:
  base::Version GetCurrentVersion() const;
  bool IsUserEnterpriseManaged() const;
  MOCK_METHOD0(ShowUpdateRequiredScreen, void());
  MOCK_METHOD0(RestartToLoginScreen, void());
  MOCK_METHOD0(HideUpdateRequiredScreenIfShown, void());
  MOCK_CONST_METHOD0(IsLoginSessionState, bool());
  MOCK_CONST_METHOD0(IsKioskMode, bool());
  MOCK_CONST_METHOD0(IsLoginInProgress, bool());
  MOCK_CONST_METHOD0(IsEnterpriseManaged, bool());
  MOCK_CONST_METHOD0(IsUserLoggedIn, bool());

  void SetCurrentVersionString(const std::string& version);

  void CreateMinimumVersionHandler();
  const MinimumVersionRequirement* GetState() const;

  // Set new value for policy pref.
  void SetPolicyPref(base::Value value);

  void VerifyUpdateRequiredNotification(const base::string16& expected_title,
                                        const base::string16& expected_message);

  policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
    return minimum_version_policy_handler_.get();
  }

  NotificationDisplayServiceTester* display_service() {
    return notification_service_.get();
  }

  chromeos::FakeUpdateEngineClient* update_engine() {
    return fake_update_engine_client_;
  }

  void SetUserManaged(bool managed) { user_managed_ = managed; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  bool user_managed_ = true;
  ScopedTestingLocalState local_state_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_service_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;
  std::unique_ptr<base::Version> current_version_;
  std::unique_ptr<policy::MinimumVersionPolicyHandler>
      minimum_version_policy_handler_;
};

UpdateRequiredNotificationTest::UpdateRequiredNotificationTest()
    : local_state_(TestingBrowserProcess::GetGlobal()) {
  ON_CALL(*this, IsEnterpriseManaged).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsUserLoggedIn).WillByDefault(testing::Return(true));
}

void UpdateRequiredNotificationTest::SetUp() {
  auto fake_update_engine_client =
      std::make_unique<chromeos::FakeUpdateEngineClient>();
  fake_update_engine_client_ = fake_update_engine_client.get();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
      std::move(fake_update_engine_client));
  chromeos::NetworkHandler::Initialize();

  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();
  service_test->AddService("/service/eth", "eth" /* guid */, "eth",
                           shill::kTypeEthernet, shill::kStateOnline,
                           true /* visible */);
  base::RunLoop().RunUntilIdle();

  scoped_stub_install_attributes_.Get()->SetCloudManaged("managed.com",
                                                         "device_id");
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  notification_service_ =
      std::make_unique<NotificationDisplayServiceTester>(nullptr /*profile*/);

  CreateMinimumVersionHandler();
  SetCurrentVersionString(kFakeCurrentVersion);
}

void UpdateRequiredNotificationTest::TearDown() {
  minimum_version_policy_handler_.reset();
  chromeos::NetworkHandler::Shutdown();
}

void UpdateRequiredNotificationTest::CreateMinimumVersionHandler() {
  minimum_version_policy_handler_.reset(new policy::MinimumVersionPolicyHandler(
      this, chromeos::CrosSettings::Get()));
}

const MinimumVersionRequirement* UpdateRequiredNotificationTest::GetState()
    const {
  return minimum_version_policy_handler_->GetState();
}

void UpdateRequiredNotificationTest::SetCurrentVersionString(
    const std::string& version) {
  current_version_.reset(new base::Version(version));
  ASSERT_TRUE(current_version_->IsValid());
}

bool UpdateRequiredNotificationTest::IsUserEnterpriseManaged() const {
  return user_managed_;
}

base::Version UpdateRequiredNotificationTest::GetCurrentVersion() const {
  return *current_version_;
}

void UpdateRequiredNotificationTest::SetPolicyPref(base::Value value) {
  scoped_testing_cros_settings_.device_settings()->Set(
      chromeos::kDeviceMinimumVersion, value);
}

void UpdateRequiredNotificationTest::VerifyUpdateRequiredNotification(
    const base::string16& expected_title,
    const base::string16& expected_message) {
  auto notification =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);
}

TEST_F(UpdateRequiredNotificationTest, NoNetworkNotifications) {
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // Disconnect all networks
  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(policy::CreateMinimumVersionSingleRequirementPolicyValue(
      kNewVersion, kLongWarningInDays, kLongWarningInDays,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // Check notification is shown for offline devices with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Update Chrome device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to download an update before the deadline. The "
      "update will download automatically when you connect to the internet.");
  VerifyUpdateRequiredNotification(expected_title, expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning =
      base::TimeDelta::FromDays(kLongWarningInDays - 1);
  task_environment_.FastForwardBy(warning);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update Chrome device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to download an update today. The "
      "update will download automatically when you connect to the internet.");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(UpdateRequiredNotificationTest, MeteredNetworkNotifications) {
  // Connect to metered network
  chromeos::ShillServiceClient::TestInterface* service_test =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  service_test->ClearServices();
  service_test->AddService(kCellularServicePath,
                           kCellularServicePath /* guid */,
                           kCellularServicePath, shill::kTypeCellular,
                           shill::kStateOnline, true /* visible */);
  base::RunLoop().RunUntilIdle();

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(policy::CreateMinimumVersionSingleRequirementPolicyValue(
      kNewVersion, kLongWarningInDays, kLongWarningInDays,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Check notification is shown for metered network with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Update Chrome device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to connect to Wi-Fi and download an update "
      "before the deadline. Or, download from a metered connection (charges "
      "may apply).");
  VerifyUpdateRequiredNotification(expected_title, expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning =
      base::TimeDelta::FromDays(kLongWarningInDays - 1);
  task_environment_.FastForwardBy(warning);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update Chrome device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to connect to Wi-Fi today to download an "
      "update. Or, download from a metered connection (charges may apply).");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(UpdateRequiredNotificationTest, EolNotifications) {
  // Set device state to end of life.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(1));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(policy::CreateMinimumVersionSingleRequirementPolicyValue(
      kNewVersion, kLongWarningInDays, kLongWarningInDays,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Check notification is shown for end of life with the warning time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Return Chrome device within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this Chrome "
      "device before the deadline.");
  VerifyUpdateRequiredNotification(expected_title, expected_message);

  // Expire notification timer to show new notification a week before deadline.
  const base::TimeDelta warning =
      base::TimeDelta::FromDays(kLongWarningInDays - 7);
  task_environment_.FastForwardBy(warning);
  base::string16 expected_title_one_week =
      base::ASCIIToUTF16("Return Chrome device within 1 week");
  VerifyUpdateRequiredNotification(expected_title_one_week, expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning_last_day = base::TimeDelta::FromDays(6);
  task_environment_.FastForwardBy(warning_last_day);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Immediate return required");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this Chrome "
      "device today.");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(UpdateRequiredNotificationTest, LastHourEolNotifications) {
  // Set device state to end of life.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(kLongWarningInDays));

  // Set local state to simulate update required timer running and one hour to
  // deadline.
  PrefService* prefs = g_browser_process->local_state();
  const base::TimeDelta delta = base::TimeDelta::FromDays(kShortWarningInDays) -
                                base::TimeDelta::FromHours(1);
  prefs->SetTime(prefs::kUpdateRequiredTimerStartTime,
                 base::Time::Now() - delta);
  prefs->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod,
                      base::TimeDelta::FromDays(kShortWarningInDays));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(policy::CreateMinimumVersionSingleRequirementPolicyValue(
      kNewVersion, kShortWarningInDays, kShortWarningInDays,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Immediate return required");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this Chrome "
      "device today.");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(UpdateRequiredNotificationTest, ChromeboxNotifications) {
  base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                               base::Time::Now());
  // Set device state to end of life reached.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(kLongWarningInDays));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(policy::CreateMinimumVersionSingleRequirementPolicyValue(
      kNewVersion, kLongWarningInDays, kLongWarningInDays,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Check Chromebox notification is shown for end of life with the warning
  // time.
  base::string16 expected_title =
      base::ASCIIToUTF16("Return Chromebox within 10 days");
  base::string16 expected_message = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this Chromebox "
      "before the deadline.");
  VerifyUpdateRequiredNotification(expected_title, expected_message);

  // Expire notification timer to show new notification a week before deadline.
  const base::TimeDelta warning =
      base::TimeDelta::FromDays(kLongWarningInDays - 7);
  task_environment_.FastForwardBy(warning);
  base::string16 expected_title_one_week =
      base::ASCIIToUTF16("Return Chromebox within 1 week");
  VerifyUpdateRequiredNotification(expected_title_one_week, expected_message);
}

}  // namespace chromeos
