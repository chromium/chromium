// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/status_collector/child_status_collector.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
namespace em = enterprise_management;

using ::base::Time;
using ::base::TimeDelta;
using ::testing::Return;
using ::testing::ReturnRef;

// Time delta representing midnight 00:00.
constexpr TimeDelta kMidnight;

// Time delta representing 06:00AM.
constexpr TimeDelta kSixAm = TimeDelta::FromHours(6);

// Time delta representing 1 hour time interval.
constexpr TimeDelta kHour = TimeDelta::FromHours(1);

constexpr int64_t kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;

const char kArcStatus[] = R"(
{
   "applications":[
      {
         "packageName":"com.android.providers.telephony",
         "versionName":"6.0.1",
         "permissions":[ "android.permission.INTERNET" ]
      }
   ],
   "userEmail":"xxx@google.com"
})";
const char kDroidGuardInfo[] = "{\"droid_guard_info\":42}";

const char kFakeDmToken[] = "kFakeDmToken";

class TestingChildStatusCollector : public policy::ChildStatusCollector {
 public:
  TestingChildStatusCollector(
      PrefService* pref_service,
      chromeos::system::StatisticsProvider* provider,
      const policy::ChildStatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      TimeDelta activity_day_start)
      : policy::ChildStatusCollector(pref_service,
                                     provider,
                                     android_status_fetcher,
                                     activity_day_start) {
    // Set the baseline time to a fixed value (1 hour after day start) to
    // prevent test flakiness due to a single activity period spanning two days.
    // TODO(crbug.com/827386): migrate to use SimpleTestClock.
    SetBaselineTime(Time::Now().LocalMidnight() + activity_day_start + kHour);
  }

  void UpdateUsageTime() { UpdateChildUsageTime(); }

  // Reset the baseline time.
  void SetBaselineTime(Time time) {
    baseline_time_ = time;
    baseline_offset_periods_ = 0;
  }

  std::string GetDMTokenForProfile(Profile* profile) const override {
    return kFakeDmToken;
  }

 protected:
  // Each time this is called, returns a time that is a fixed increment
  // later than the previous time.
  Time GetCurrentTime() override {
    int poll_interval = policy::ChildStatusCollector::kIdlePollIntervalSeconds;
    return baseline_time_ +
           TimeDelta::FromSeconds(poll_interval * baseline_offset_periods_++);
  }

 private:
  // Baseline time for the fake times returned from GetCurrentTime().
  Time baseline_time_;

  // The number of simulated periods since the baseline time.
  int baseline_offset_periods_;
};

// Return the total number of active milliseconds contained in a device
// status report.
int64_t GetActiveMilliseconds(const em::DeviceStatusReportRequest& status) {
  int64_t active_milliseconds = 0;
  for (int i = 0; i < status.active_periods_size(); i++) {
    active_milliseconds += status.active_periods(i).active_duration();
  }
  return active_milliseconds;
}

// Overloads |GetActiveMilliseconds| for child status report.
int64_t GetActiveMilliseconds(const em::ChildStatusReportRequest& status) {
  int64_t active_milliseconds = 0;
  for (int i = 0; i < status.screen_time_span_size(); i++) {
    active_milliseconds += status.screen_time_span(i).active_duration_ms();
  }
  return active_milliseconds;
}

void CallAndroidStatusReceiver(
    const policy::ChildStatusCollector::AndroidStatusReceiver& receiver,
    const std::string& status,
    const std::string& droid_guard_info) {
  receiver.Run(status, droid_guard_info);
}

bool GetEmptyAndroidStatus(
    const policy::ChildStatusCollector::AndroidStatusReceiver& receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, receiver, "", ""));
  return true;
}

bool GetFakeAndroidStatus(
    const std::string& status,
    const std::string& droid_guard_info,
    const policy::ChildStatusCollector::AndroidStatusReceiver& receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, receiver, status,
                                droid_guard_info));
  return true;
}

}  // namespace

namespace policy {

// Though it is a unit test, this test is linked with browser_tests so that it
// runs in a separate process. The intention is to avoid overriding the timezone
// environment variable for other tests.
class ChildStatusCollectorTest : public testing::Test {
 public:
  ChildStatusCollectorTest()
      : user_manager_(new chromeos::MockUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_)),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        update_engine_client_(new chromeos::FakeUpdateEngineClient) {
    scoped_stub_install_attributes_.Get()->SetCloudManaged("managed.com",
                                                           "device_id");
    EXPECT_CALL(*user_manager_, Shutdown()).Times(1);

    // Ensure mojo is started, otherwise browser context keyed services that
    // rely on mojo will explode.
    mojo::core::Init();

    // Although this is really a unit test which runs in the browser_tests
    // binary, it doesn't get the unit setup which normally happens in the unit
    // test binary.
    ChromeUnitTestSuite::InitializeProviders();
    ChromeUnitTestSuite::InitializeResourceBundle();

    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_content_client_);

    // Run this test with a well-known timezone so that Time::LocalMidnight()
    // returns the same values on all machines.
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("TZ", "UTC");

    TestingChildStatusCollector::RegisterProfilePrefs(
        profile_pref_service_.registry());

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);

    // Use FakeUpdateEngineClient.
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        base::WrapUnique<chromeos::UpdateEngineClient>(update_engine_client_));

    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();

    MockChildUser(AccountId::FromUserEmail("user0@gmail.com"));
  }

  ~ChildStatusCollectorTest() override {
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    // Finish pending tasks.
    content::RunAllTasksUntilIdle();
  }

  void SetUp() override {
    RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));

    // Disable network interface reporting since it requires additional setup.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kReportDeviceNetworkInterfaces, false);
  }

  void TearDown() override { status_collector_.reset(); }

 protected:
  // States tracked to calculate a child's active time.
  enum class DeviceStateTransitions {
    kEnterIdleState,
    kLeaveIdleState,
    kEnterSleep,
    kLeaveSleep,
    kEnterSessionActive,
    kLeaveSessionActive,
    kPeriodicCheckTriggered
  };

  void SimulateStateChanges(DeviceStateTransitions* states, int len) {
    for (int i = 0; i < len; i++) {
      switch (states[i]) {
        case DeviceStateTransitions::kEnterIdleState: {
          power_manager::ScreenIdleState state;
          state.set_off(true);
          chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
              state);
        } break;
        case DeviceStateTransitions::kLeaveIdleState: {
          power_manager::ScreenIdleState state;
          state.set_off(false);
          chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
              state);
        } break;
        case DeviceStateTransitions::kEnterSleep:
          chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
              power_manager::SuspendImminent_Reason_LID_CLOSED);
          break;
        case DeviceStateTransitions::kLeaveSleep:
          chromeos::FakePowerManagerClient::Get()->SendSuspendDone(
              base::TimeDelta::FromSeconds(
                  policy::ChildStatusCollector::kIdlePollIntervalSeconds));
          break;
        case DeviceStateTransitions::kEnterSessionActive:
          session_manager::SessionManager::Get()->SetSessionState(
              session_manager::SessionState::ACTIVE);
          break;
        case DeviceStateTransitions::kLeaveSessionActive:
          session_manager::SessionManager::Get()->SetSessionState(
              session_manager::SessionState::LOCKED);
          break;
        case DeviceStateTransitions::kPeriodicCheckTriggered:
          status_collector_->UpdateUsageTime();
          break;
      }
    }
  }

  virtual void RestartStatusCollector(
      const policy::ChildStatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      const TimeDelta activity_day_start = kMidnight) {
    status_collector_ = std::make_unique<TestingChildStatusCollector>(
        &profile_pref_service_, &fake_statistics_provider_,
        android_status_fetcher, activity_day_start);
  }

  void GetStatus() {
    device_status_.Clear();
    session_status_.Clear();
    run_loop_.reset(new base::RunLoop());
    status_collector_->GetStatusAsync(base::BindRepeating(
        &ChildStatusCollectorTest::OnStatusReceived, base::Unretained(this)));
    run_loop_->Run();
    run_loop_.reset();
  }

  void OnStatusReceived(StatusCollectorParams callback_params) {
    if (callback_params.device_status)
      device_status_ = *callback_params.device_status;
    if (callback_params.session_status)
      session_status_ = *callback_params.session_status;
    if (callback_params.child_status)
      child_status_ = *callback_params.child_status;
    EXPECT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void MockUserWithTypeAndAffiliation(const AccountId& account_id,
                                      user_manager::UserType user_type,
                                      bool is_affiliated) {
    user_manager_->AddUserWithAffiliationAndType(account_id, is_affiliated,
                                                 user_type);
    // The user just added will be the active user because there's only one
    // user.
    user_manager::User* user = user_manager_->GetActiveUser();

    // Build a profile with profile name=account e-mail because our testing
    // version of GetDMTokenForProfile returns the profile name.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    testing_profile_ = profile_builder.Build();
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());

    EXPECT_CALL(*user_manager_, IsLoggedInAsKioskApp())
        .WillRepeatedly(Return(false));
  }

  void MockChildUser(const AccountId& account_id) {
    MockUserWithTypeAndAffiliation(account_id, user_manager::USER_TYPE_CHILD,
                                   false);
    EXPECT_CALL(*user_manager_, IsLoggedInAsChildUser())
        .WillRepeatedly(Return(true));
  }

  void MockPlatformVersion(const std::string& platform_version) {
    const std::string lsb_release = base::StringPrintf(
        "CHROMEOS_RELEASE_VERSION=%s", platform_version.c_str());
    base::SysInfo::SetChromeOSVersionInfoForTest(lsb_release,
                                                 base::Time::Now());
  }

  // Convenience method.
  int64_t ActivePeriodMilliseconds() {
    return policy::ChildStatusCollector::kIdlePollIntervalSeconds * 1000;
  }

  void ExpectChildScreenTimeMilliseconds(int64_t duration) {
    profile_pref_service_.CommitPendingWrite(
        base::OnceClosure(),
        base::BindOnce(
            [](int64_t duration,
               TestingPrefServiceSimple* profile_pref_service_) {
              EXPECT_EQ(duration, profile_pref_service_->GetInteger(
                                      prefs::kChildScreenTimeMilliseconds));
            },
            duration, &profile_pref_service_));
  }

  void ExpectLastChildScreenTimeReset(Time time) {
    profile_pref_service_.CommitPendingWrite(
        base::OnceClosure(),
        base::BindOnce(
            [](Time time, TestingPrefServiceSimple* profile_pref_service_) {
              EXPECT_EQ(time, profile_pref_service_->GetTime(
                                  prefs::kLastChildScreenTimeReset));
            },
            time, &profile_pref_service_));
  }

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;
  content::BrowserTaskEnvironment task_environment_;

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::FakeOwnerSettingsService owner_settings_service_{
      scoped_testing_cros_settings_.device_settings(), nullptr};
  std::unique_ptr<TestingProfile> testing_profile_;
  chromeos::MockUserManager* const user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  em::DeviceStatusReportRequest device_status_;
  em::SessionStatusReportRequest session_status_;
  em::ChildStatusReportRequest child_status_;
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple profile_pref_service_;
  std::unique_ptr<TestingChildStatusCollector> status_collector_;
  base::ScopedPathOverride user_data_dir_override_;
  chromeos::FakeUpdateEngineClient* const update_engine_client_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildStatusCollectorTest);
};

TEST_F(ChildStatusCollectorTest, ReportingBootMode) {
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueVerified);

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  EXPECT_TRUE(device_status_.has_boot_mode());
  EXPECT_EQ("Verified", device_status_.boot_mode());
  // END.

  EXPECT_TRUE(child_status_.has_boot_mode());
  EXPECT_EQ("Verified", child_status_.boot_mode());
}

// TODO(crbug.com/827386): remove after migration.
TEST_F(ChildStatusCollectorTest, NotReportingWriteProtectSwitch) {
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey,
      chromeos::system::kFirmwareWriteProtectBootValueOn);

  GetStatus();

  EXPECT_FALSE(device_status_.has_write_protect_switch());
}
// END.

TEST_F(ChildStatusCollectorTest, ReportingArcStatus) {
  RestartStatusCollector(
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo));
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  EXPECT_EQ(kArcStatus, session_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo,
            session_status_.android_status().droid_guard_info());
  EXPECT_EQ(kFakeDmToken, session_status_.user_dm_token());
  // END.

  EXPECT_EQ(kArcStatus, child_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo, child_status_.android_status().droid_guard_info());
  EXPECT_EQ(kFakeDmToken, child_status_.user_dm_token());
}

TEST_F(ChildStatusCollectorTest, ReportingPartialVersionInfo) {
  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  // Should only report OS version.
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_FALSE(device_status_.has_browser_version());
  EXPECT_FALSE(device_status_.has_channel());
  EXPECT_FALSE(device_status_.has_firmware_version());
  EXPECT_FALSE(device_status_.has_tpm_version_info());
  // END.

  EXPECT_TRUE(child_status_.has_os_version());
}

// TODO(crbug.com/827386): remove after migration.
TEST_F(ChildStatusCollectorTest, NotReportingVolumeInfo) {
  RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));
  content::RunAllTasksUntilIdle();

  GetStatus();

  EXPECT_EQ(0, device_status_.volume_infos_size());
}

TEST_F(ChildStatusCollectorTest, NotReportingUsers) {
  const AccountId account_id0(AccountId::FromUserEmail("user0@gmail.com"));
  const AccountId account_id1(AccountId::FromUserEmail("user1@gmail.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id1, true,
                                               user_manager::USER_TYPE_CHILD);

  GetStatus();

  EXPECT_EQ(0, device_status_.users_size());
}

TEST_F(ChildStatusCollectorTest, NotReportingOSUpdateStatus) {
  MockPlatformVersion("1234.0.0");

  GetStatus();

  EXPECT_FALSE(device_status_.has_os_update_status());
}

TEST_F(ChildStatusCollectorTest, NotReportingDeviceHardwareStatus) {
  EXPECT_FALSE(device_status_.has_sound_volume());
  EXPECT_EQ(0, device_status_.cpu_utilization_pct_samples().size());
  EXPECT_EQ(0, device_status_.cpu_temp_infos_size());
  EXPECT_EQ(0, device_status_.system_ram_free_samples().size());
  EXPECT_FALSE(device_status_.has_system_ram_total());
  EXPECT_FALSE(device_status_.has_tpm_status_info());
}

TEST_F(ChildStatusCollectorTest, TimeZoneReporting) {
  const std::string timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  EXPECT_TRUE(session_status_.has_time_zone());
  EXPECT_EQ(timezone, session_status_.time_zone());
  // END.

  EXPECT_TRUE(child_status_.has_time_zone());
  EXPECT_EQ(timezone, child_status_.time_zone());
}

TEST_F(ChildStatusCollectorTest, ReportingActivityTimesSessionTransistions) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  ASSERT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ("",  // No email should be saved for child account.
            device_status_.active_periods(0).user_email());
  // END.

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ReportingActivityTimesSleepTransistions) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kEnterSleep,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kLeaveSleep,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kEnterSleep,
      DeviceStateTransitions::kLeaveSleep,
      DeviceStateTransitions::kPeriodicCheckTriggered};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  ASSERT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ("",  // No email should be saved for child account.
            device_status_.active_periods(0).user_email());
  // END.

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(4 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ReportingActivityTimesIdleTransitions) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kEnterIdleState,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kLeaveIdleState,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  ASSERT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ("",  // No email should be saved for child account.
            device_status_.active_periods(0).user_email());
  // END.

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ActivityKeptInPref) {
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());
  base::Time initial_time = base::Time::Now() + kHour;
  status_collector_->SetBaselineTime(initial_time);

  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));
  EXPECT_FALSE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));
  status_collector_->SetBaselineTime(initial_time);
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  EXPECT_EQ(12 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  // END.

  ExpectChildScreenTimeMilliseconds(12 * ActivePeriodMilliseconds());
  EXPECT_EQ(12 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
}

TEST_F(ChildStatusCollectorTest, ActivityNotWrittenToLocalState) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));
  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  // END.

  EXPECT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
  // Nothing should be written to local state, because it is only used for
  // enterprise reporting.
}

TEST_F(ChildStatusCollectorTest, BeforeDayStart) {
  RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus), kSixAm);
  // 04:00 AM
  Time initial_time = Time::Now().LocalMidnight() + TimeDelta::FromHours(4);
  status_collector_->SetBaselineTime(initial_time);
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());

  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kLeaveSessionActive,
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));
  GetStatus();
  // 4 is the number of states yielding an active period with duration of
  // ActivePeriodMilliseconds
  // TODO(crbug.com/827386): remove after migration.
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  // END.
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(4 * ActivePeriodMilliseconds());
  ExpectLastChildScreenTimeReset(initial_time);
}

TEST_F(ChildStatusCollectorTest, ActivityCrossingMidnight) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kLeaveSessionActive};

  // Set the baseline time to 15 seconds before midnight, so the activity is
  // split between two days.
  status_collector_->SetBaselineTime(Time::Now().LocalMidnight() -
                                     TimeDelta::FromSeconds(15));
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));
  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  ASSERT_EQ(2, device_status_.active_periods_size());

  em::ActiveTimePeriod period0 = device_status_.active_periods(0);
  em::ActiveTimePeriod period1 = device_status_.active_periods(1);
  EXPECT_EQ(ActivePeriodMilliseconds() - 15000, period0.active_duration());
  EXPECT_EQ(15000, period1.active_duration());

  em::TimePeriod time_period0 = period0.time_period();
  em::TimePeriod time_period1 = period1.time_period();

  EXPECT_EQ(time_period0.end_timestamp(), time_period1.start_timestamp());

  // Ensure that the start and end times for the period are a day apart.
  EXPECT_EQ(time_period0.end_timestamp() - time_period0.start_timestamp(),
            kMillisecondsPerDay);
  EXPECT_EQ(time_period1.end_timestamp() - time_period1.start_timestamp(),
            kMillisecondsPerDay);
  // END.

  ASSERT_EQ(2, child_status_.screen_time_span_size());

  em::ScreenTimeSpan timespan0 = child_status_.screen_time_span(0);
  em::ScreenTimeSpan timespan1 = child_status_.screen_time_span(1);
  EXPECT_EQ(ActivePeriodMilliseconds() - 15000, timespan0.active_duration_ms());
  EXPECT_EQ(15000, timespan1.active_duration_ms());

  em::TimePeriod timespan0period = timespan0.time_period();
  em::TimePeriod timespan1period = timespan1.time_period();

  EXPECT_EQ(timespan0period.end_timestamp(), timespan1period.start_timestamp());

  // Ensure that the start and end times for the period are a day apart.
  EXPECT_EQ(timespan0period.end_timestamp() - timespan0period.start_timestamp(),
            kMillisecondsPerDay);
  EXPECT_EQ(timespan1period.end_timestamp() - timespan1period.start_timestamp(),
            kMillisecondsPerDay);
  ExpectChildScreenTimeMilliseconds(0.5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ClockChanged) {
  DeviceStateTransitions test_states[1] = {
      DeviceStateTransitions::kEnterSessionActive};
  base::Time initial_time =
      Time::Now().LocalMidnight() + base::TimeDelta::FromHours(1);
  status_collector_->SetBaselineTime(initial_time);
  SimulateStateChanges(test_states, 1);

  // Simulate clock change.
  status_collector_->SetBaselineTime(initial_time - TimeDelta::FromMinutes(30));
  test_states[0] = DeviceStateTransitions::kLeaveSessionActive;
  SimulateStateChanges(test_states, 1);

  GetStatus();

  // TODO(crbug.com/827386): remove after migration.
  ASSERT_EQ(1, device_status_.active_periods_size());
  // END.
  ASSERT_EQ(1, child_status_.screen_time_span_size());
  ExpectChildScreenTimeMilliseconds(ActivePeriodMilliseconds());
}

}  // namespace policy
