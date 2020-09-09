// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_features.h"
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

namespace policy {

namespace {
const char kFakeCurrentVersion[] = "13305.20.0";
const char kNewVersion[] = "13305.25.0";
const char kNewerVersion[] = "13310.0.0";
const char kNewestVersion[] = "13320.10.0";
const char kOldVersion[] = "13301.0.0";
const char kUpdateRequiredNotificationId[] = "policy.update_required";
const char kCellularServicePath[] = "/service/cellular1";

const int kLongWarning = 10;
const int kShortWarning = 2;
const int kNoWarning = 0;

}  // namespace

class MinimumVersionPolicyHandlerTest
    : public testing::Test,
      public MinimumVersionPolicyHandler::Delegate {
 public:
  MinimumVersionPolicyHandlerTest();

  void SetUp() override;
  void TearDown() override;

  // MinimumVersionPolicyHandler::Delegate:
  bool IsKioskMode() const;
  bool IsEnterpriseManaged() const;
  base::Version GetCurrentVersion() const;
  bool IsUserEnterpriseManaged() const;
  bool IsUserLoggedIn() const;
  bool IsLoginInProgress() const;
  MOCK_METHOD0(ShowUpdateRequiredScreen, void());
  MOCK_METHOD0(RestartToLoginScreen, void());
  MOCK_METHOD0(HideUpdateRequiredScreenIfShown, void());
  MOCK_CONST_METHOD0(IsLoginSessionState, bool());

  void SetCurrentVersionString(std::string version);

  void CreateMinimumVersionHandler();
  const MinimumVersionPolicyHandler::MinimumVersionRequirement* GetState()
      const;

  // Set new value for policy pref.
  void SetPolicyPref(base::Value value);

  // Create a new requirement as a dictionary to be used in the policy value.
  base::Value CreateRequirement(const std::string& version,
                                int warning,
                                int eol_warning) const;

  base::Value CreatePolicyValue(base::Value requirements,
                                bool unmanaged_user_restricted);

  base::Value CreateSingleRequirementPolicyValue(
      const std::string& version,
      int warning,
      int eol_warning,
      bool unmanaged_user_restricted);

  void VerifyUpdateRequiredNotification(const base::string16& expected_title,
                                        const base::string16& expected_message);

  MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
    return minimum_version_policy_handler_.get();
  }

  NotificationDisplayServiceTester* display_service() {
    return notification_service_.get();
  }

  chromeos::FakeUpdateEngineClient* update_engine() {
    return fake_update_engine_client_;
  }

  void SetUserManaged(bool managed) { user_managed_ = managed; }

  content::BrowserTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  bool user_managed_ = true;
  ScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList feature_list_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_service_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_;
  std::unique_ptr<base::Version> current_version_;
  std::unique_ptr<MinimumVersionPolicyHandler> minimum_version_policy_handler_;
};

MinimumVersionPolicyHandlerTest::MinimumVersionPolicyHandlerTest()
    : local_state_(TestingBrowserProcess::GetGlobal()) {
  feature_list_.InitAndEnableFeature(chromeos::features::kMinimumChromeVersion);
}

void MinimumVersionPolicyHandlerTest::SetUp() {
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

void MinimumVersionPolicyHandlerTest::TearDown() {
  minimum_version_policy_handler_.reset();
  chromeos::NetworkHandler::Shutdown();
}

void MinimumVersionPolicyHandlerTest::CreateMinimumVersionHandler() {
  minimum_version_policy_handler_.reset(
      new MinimumVersionPolicyHandler(this, chromeos::CrosSettings::Get()));
}

const MinimumVersionRequirement* MinimumVersionPolicyHandlerTest::GetState()
    const {
  return minimum_version_policy_handler_->GetState();
}

void MinimumVersionPolicyHandlerTest::SetCurrentVersionString(
    std::string version) {
  current_version_.reset(new base::Version(version));
  ASSERT_TRUE(current_version_->IsValid());
}

bool MinimumVersionPolicyHandlerTest::IsKioskMode() const {
  return false;
}

bool MinimumVersionPolicyHandlerTest::IsEnterpriseManaged() const {
  return true;
}

bool MinimumVersionPolicyHandlerTest::IsUserEnterpriseManaged() const {
  return user_managed_;
}

bool MinimumVersionPolicyHandlerTest::IsUserLoggedIn() const {
  return true;
}

bool MinimumVersionPolicyHandlerTest::IsLoginInProgress() const {
  return false;
}

base::Version MinimumVersionPolicyHandlerTest::GetCurrentVersion() const {
  return *current_version_;
}

void MinimumVersionPolicyHandlerTest::SetPolicyPref(base::Value value) {
  scoped_testing_cros_settings_.device_settings()->Set(
      chromeos::kDeviceMinimumVersion, value);
}

/**
 *  Create a dictionary value to represent minimum version requirement.
 *  @param version The minimum required version in string form.
 *  @param warning The warning period in days.
 *  @param eol_warning The end of life warning period in days.
 */
base::Value MinimumVersionPolicyHandlerTest::CreateRequirement(
    const std::string& version,
    const int warning,
    const int eol_warning) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(MinimumVersionPolicyHandler::kChromeOsVersion, version);
  dict.SetIntKey(MinimumVersionPolicyHandler::kWarningPeriod, warning);
  dict.SetIntKey(MinimumVersionPolicyHandler::kEolWarningPeriod, eol_warning);
  return dict;
}

base::Value MinimumVersionPolicyHandlerTest::CreatePolicyValue(
    base::Value requirements,
    bool unmanaged_user_restricted) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(MinimumVersionPolicyHandler::kRequirements,
              std::move(requirements));
  dict.SetBoolKey(MinimumVersionPolicyHandler::kUnmanagedUserRestricted,
                  unmanaged_user_restricted);
  return dict;
}

base::Value MinimumVersionPolicyHandlerTest::CreateSingleRequirementPolicyValue(
    const std::string& version,
    int warning,
    int eol_warning,
    bool unmanaged_user_restricted) {
  base::Value requirement_list(base::Value::Type::LIST);
  requirement_list.Append(CreateRequirement(version, warning, eol_warning));
  return CreatePolicyValue(std::move(requirement_list),
                           unmanaged_user_restricted);
}

void MinimumVersionPolicyHandlerTest::VerifyUpdateRequiredNotification(
    const base::string16& expected_title,
    const base::string16& expected_message) {
  auto notification =
      display_service()->GetNotification(kUpdateRequiredNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);
}

TEST_F(MinimumVersionPolicyHandlerTest, RequirementsNotMetState) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->GetTimeRemainingInDays());

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value new_version_short_warning =
      CreateRequirement(kNewVersion, kShortWarning, kNoWarning);
  auto strongest_requirement = MinimumVersionRequirement::CreateInstanceIfValid(
      &base::Value::AsDictionaryValue(new_version_short_warning));

  requirement_list.Append(std::move(new_version_short_warning));
  requirement_list.Append(
      CreateRequirement(kNewerVersion, kLongWarning, kNoWarning));
  requirement_list.Append(
      CreateRequirement(kNewestVersion, kNoWarning, kNoWarning));

  // Set new value for pref and check that requirements are not satisfied.
  // The state in |MinimumVersionPolicyHandler| should be equal to the strongest
  // requirement as defined in the policy description.
  SetPolicyPref(CreatePolicyValue(std::move(requirement_list),
                                  false /* unmanaged_user_restricted */));
  run_loop.Run();

  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
  EXPECT_TRUE(strongest_requirement);
  EXPECT_EQ(GetState()->Compare(strongest_requirement.get()), 0);
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetTimeRemainingInDays());
  EXPECT_EQ(GetMinimumVersionPolicyHandler()->GetTimeRemainingInDays().value(),
            kShortWarning);

  // Reset the pref to empty list and verify state is reset.
  base::Value requirement_list2(base::Value::Type::LIST);
  SetPolicyPref(std::move(requirement_list2));
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->GetTimeRemainingInDays());
}

TEST_F(MinimumVersionPolicyHandlerTest, CriticalUpdates) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  base::RunLoop run_loop;
  // Expect calls to make sure that user is logged out.
  EXPECT_CALL(*this, RestartToLoginScreen())
      .Times(1)
      .WillOnce(testing::Invoke([&run_loop]() {
        run_loop.Quit();
        return false;
      }));
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);
  EXPECT_CALL(*this, HideUpdateRequiredScreenIfShown()).Times(0);
  EXPECT_CALL(*this, IsLoginSessionState())
      .Times(1)
      .WillOnce(testing::Return(false));

  // Set new value for pref and check that requirements are not satisfied.
  // As the warning time is set to zero, the user should be logged out of the
  // session.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kNoWarning, kLongWarning,
      false /* unmanaged_user_restricted */));
  // Start the run loop to wait for EOL status fetch.
  run_loop.Run();
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, CriticalUpdatesUnmanagedUser) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  base::RunLoop run_loop;
  // Expect calls to make sure that user is not logged out.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);
  EXPECT_CALL(*this, HideUpdateRequiredScreenIfShown()).Times(0);
  // Unmanaged user is not logged out of the session. The run loop is quit on
  // reaching IsLoginSessionState() because that implies we have fetched the
  // EOL status and reached the end of the policy handler code flow.
  EXPECT_CALL(*this, IsLoginSessionState())
      .Times(1)
      .WillOnce(testing::Invoke([&run_loop]() {
        run_loop.Quit();
        return false;
      }));

  // Set user as unmanaged.
  SetUserManaged(false);

  // Set new value for pref and check that requirements are not satisfied.
  // Unmanaged user should not be logged out of the session.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kNoWarning, kLongWarning,
      false /* unmanaged_user_restricted */));
  // Start the run loop to wait for EOL status fetch.
  run_loop.Run();
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_TRUE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, RequirementsMetState) {
  // No policy applied yet. Check requirements are satisfied.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());

  // Create policy value as a list of requirements.
  base::Value requirement_list(base::Value::Type::LIST);
  base::Value current_version_no_warning =
      CreateRequirement(kFakeCurrentVersion, kNoWarning, kNoWarning);
  base::Value old_version_long_warning =
      CreateRequirement(kOldVersion, kLongWarning, kNoWarning);
  requirement_list.Append(std::move(current_version_no_warning));
  requirement_list.Append(std::move(old_version_long_warning));

  // Set new value for pref and check that requirements are still satisfied
  // as none of the requirements has version greater than current version.
  SetPolicyPref(CreatePolicyValue(std::move(requirement_list),
                                  false /* unmanaged_user_restricted */));
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
  EXPECT_FALSE(GetState());
}

TEST_F(MinimumVersionPolicyHandlerTest, DeadlineTimerExpired) {
  // Checks the user is logged out of the session when the deadline is reached.
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Expect calls to make sure that user is not logged out.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowUpdateRequiredScreen()).Times(0);

  // Create and set pref value to invoke policy handler such that update is
  // required with a long warning time.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kLongWarning, kLongWarning,
      false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());

  testing::Mock::VerifyAndClearExpectations(this);

  // Expire the timer and check that user is logged out of the session.
  EXPECT_CALL(*this, IsLoginSessionState()).Times(1);
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(1);
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning);
  task_environment.FastForwardBy(warning);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->RequirementsAreSatisfied());
}

TEST_F(MinimumVersionPolicyHandlerTest, NoNetworkNotifications) {
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
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kLongWarning, kLongWarning,
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
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 1);
  task_environment.FastForwardBy(warning);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update Chrome device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to download an update today. The "
      "update will download automatically when you connect to the internet.");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(MinimumVersionPolicyHandlerTest, MeteredNetworkNotifications) {
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
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kLongWarning, kLongWarning,
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
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 1);
  task_environment.FastForwardBy(warning);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Last day to update Chrome device");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to connect to Wi-Fi today to download an "
      "update. Or, download from a metered connection (charges may apply).");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(MinimumVersionPolicyHandlerTest, EolNotifications) {
  // Set device state to end of life.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(1));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kLongWarning, kLongWarning,
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
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 7);
  task_environment.FastForwardBy(warning);
  base::string16 expected_title_one_week =
      base::ASCIIToUTF16("Return Chrome device within 1 week");
  VerifyUpdateRequiredNotification(expected_title_one_week, expected_message);

  // Expire the notification timer to show new notification on the last day.
  const base::TimeDelta warning_last_day = base::TimeDelta::FromDays(6);
  task_environment.FastForwardBy(warning_last_day);
  base::string16 expected_title_last_day =
      base::ASCIIToUTF16("Immediate return required");
  base::string16 expected_message_last_day = base::ASCIIToUTF16(
      "managed.com requires you to back up your data and return this Chrome "
      "device today.");
  VerifyUpdateRequiredNotification(expected_title_last_day,
                                   expected_message_last_day);
}

TEST_F(MinimumVersionPolicyHandlerTest, LastHourEolNotifications) {
  // Set device state to end of life.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(kLongWarning));

  // Set local state to simulate update required timer running and one hour to
  // deadline.
  PrefService* prefs = g_browser_process->local_state();
  const base::TimeDelta delta =
      base::TimeDelta::FromDays(kShortWarning) - base::TimeDelta::FromHours(1);
  prefs->SetTime(prefs::kUpdateRequiredTimerStartTime,
                 base::Time::Now() - delta);
  prefs->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod,
                      base::TimeDelta::FromDays(kShortWarning));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kShortWarning, kShortWarning,
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

TEST_F(MinimumVersionPolicyHandlerTest, ChromeboxNotifications) {
  base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                               base::Time::Now());
  // Set device state to end of life reached.
  update_engine()->set_eol_date(base::DefaultClock::GetInstance()->Now() -
                                base::TimeDelta::FromDays(kLongWarning));

  // This is needed to wait till EOL status is fetched from the update_engine.
  base::RunLoop run_loop;
  GetMinimumVersionPolicyHandler()->set_fetch_eol_callback_for_testing(
      run_loop.QuitClosure());

  // Create and set pref value to invoke policy handler.
  SetPolicyPref(CreateSingleRequirementPolicyValue(
      kNewVersion, kLongWarning, kLongWarning,
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
  const base::TimeDelta warning = base::TimeDelta::FromDays(kLongWarning - 7);
  task_environment.FastForwardBy(warning);
  base::string16 expected_title_one_week =
      base::ASCIIToUTF16("Return Chromebox within 1 week");
  VerifyUpdateRequiredNotification(expected_title_one_week, expected_message);
}

}  // namespace policy
