// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/status_collector/child_status_collector.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

using ::base::Time;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

// Time delta representing midnight 00:00.
constexpr base::TimeDelta kMidnight;

// Time delta representing 06:00AM.
constexpr base::TimeDelta kSixAm = base::Hours(6);

// Time delta representing 1 hour time interval.
constexpr base::TimeDelta kHour = base::Hours(1);

constexpr int kIdlePollIntervalSeconds = 30;

constexpr char kStartTime[] = "1 Jan 2020 21:15";

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

class TestingChildStatusCollector : public ChildStatusCollector {
 public:
  TestingChildStatusCollector(
      PrefService* pref_service,
      Profile* profile,
      ash::system::StatisticsProvider* provider,
      const StatusCollector::AndroidStatusFetcher& android_status_fetcher,
      base::TimeDelta activity_day_start)
      : ChildStatusCollector(pref_service,
                             profile,
                             provider,
                             android_status_fetcher,
                             activity_day_start) {}

  // Each time this is called, returns a time that is a fixed increment
  // later than the previous time.
  void UpdateUsageTime() { UpdateChildUsageTime(); }

  std::string GetDMTokenForProfile(Profile* profile) const override {
    return kFakeDmToken;
  }
};

// Overloads |GetActiveMilliseconds| for child status report.
int64_t GetActiveMilliseconds(const em::ChildStatusReportRequest& status) {
  int64_t active_milliseconds = 0;
  for (int i = 0; i < status.screen_time_span_size(); i++) {
    active_milliseconds += status.screen_time_span(i).active_duration_ms();
  }
  return active_milliseconds;
}

void CallAndroidStatusReceiver(
    ChildStatusCollector::AndroidStatusReceiver receiver,
    const std::string& status,
    const std::string& droid_guard_info) {
  std::move(receiver).Run(status, droid_guard_info);
}

bool GetEmptyAndroidStatus(StatusCollector::AndroidStatusReceiver receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CallAndroidStatusReceiver, std::move(receiver), "", ""));
  return true;
}

bool GetFakeAndroidStatus(const std::string& status,
                          const std::string& droid_guard_info,
                          StatusCollector::AndroidStatusReceiver receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, std::move(receiver),
                                status, droid_guard_info));
  return true;
}

}  // namespace

// Though it is a unit test, this test is linked with browser_tests so that it
// runs in a separate process. The intention is to avoid overriding the timezone
// environment variable for other tests.
class ChildStatusCollectorTest : public testing::Test {
 public:
  ChildStatusCollectorTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {
    scoped_stub_install_attributes_.Get()->SetCloudManaged("managed.com",
                                                           "device_id");

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

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);

    // Use FakeUpdateEngineClient.
    ash::UpdateEngineClient::InitializeFakeForTest();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
    ash::LoginState::Initialize();

    AddChildUser(AccountId::FromUserEmail("user0@gmail.com"));
  }

  ~ChildStatusCollectorTest() override {
    ash::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    // |testing_profile_| must be destructed while ConciergeClient is alive.
    testing_profile_.reset();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::UpdateEngineClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    // Finish pending tasks.
    content::RunAllTasksUntilIdle();
  }

  ChildStatusCollectorTest(const ChildStatusCollectorTest&) = delete;
  ChildStatusCollectorTest& operator=(const ChildStatusCollectorTest&) = delete;

  void SetUp() override {
    RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));

    // Disable network reporting since it requires additional setup.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkConfiguration, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkStatus, false);

    // Mock clock in task environment is set to Unix Epoch, advance it to avoid
    // using times from before Unix Epoch in some tests.
    Time start_time;
    EXPECT_TRUE(Time::FromString(kStartTime, &start_time));
    FastForwardTo(start_time);
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

  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

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
              base::Seconds(kIdlePollIntervalSeconds));
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
          break;
      }
      task_environment_.AdvanceClock(base::Seconds(kIdlePollIntervalSeconds));
      status_collector_->UpdateUsageTime();
    }
  }

  // If `should_run_tasks` is true, then use FastForwardBy() to run tasks.
  // Otherwise use AdvanceClock() to skip running tasks.
  void SimulateAppActivity(const ash::app_time::AppId& app_id,
                           base::TimeDelta duration,
                           bool should_run_tasks = true) {
    ash::ChildUserService::TestApi child_user_service =
        ash::ChildUserService::TestApi(
            ash::ChildUserServiceFactory::GetForBrowserContext(
                testing_profile_.get()));
    EXPECT_TRUE(child_user_service.app_time_controller());

    ash::app_time::AppActivityRegistry* app_registry =
        ash::app_time::AppTimeController::TestApi(
            child_user_service.app_time_controller())
            .app_registry();
    app_registry->OnAppInstalled(app_id);

    // Window instance is irrelevant for tests here.
    auto instance_id = base::UnguessableToken::Create();
    app_registry->OnAppActive(app_id, instance_id, Time::Now());
    if (should_run_tasks) {
      task_environment_.FastForwardBy(duration);
    } else {
      task_environment_.AdvanceClock(duration);
    }
    app_registry->OnAppInactive(app_id, instance_id, Time::Now());
  }

  virtual void RestartStatusCollector(
      const StatusCollector::AndroidStatusFetcher& android_status_fetcher,
      const base::TimeDelta activity_day_start = kMidnight) {
    status_collector_ = std::make_unique<TestingChildStatusCollector>(
        pref_service(), testing_profile(), &fake_statistics_provider_,
        android_status_fetcher, activity_day_start);
  }

  void GetStatus() {
    run_loop_ = std::make_unique<base::RunLoop>();
    status_collector_->GetStatusAsync(base::BindRepeating(
        &ChildStatusCollectorTest::OnStatusReceived, base::Unretained(this)));
    run_loop_->Run();
    run_loop_.reset();
  }

  void OnStatusReceived(StatusCollectorParams callback_params) {
    if (callback_params.child_status)
      child_status_ = *callback_params.child_status;
    EXPECT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void AddUserWithTypeAndAffiliation(const AccountId& account_id,
                                     user_manager::UserType user_type,
                                     bool is_affiliated) {
    // Build a profile with profile name=account e-mail because our testing
    // version of GetDMTokenForProfile returns the profile name.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    testing_profile_ = profile_builder.Build();

    auto* user_manager = GetFakeUserManager();
    auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_type, testing_profile_.get());
    user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                               /*browser_restart=*/false, /*is_child=*/false);
  }

  void AddChildUser(const AccountId& account_id) {
    AddUserWithTypeAndAffiliation(account_id, user_manager::UserType::kChild,
                                  false);
    GetFakeUserManager()->set_current_user_child(true);
  }

  // Convenience method.
  static int64_t ActivePeriodMilliseconds() {
    return kIdlePollIntervalSeconds * base::Time::kMillisecondsPerSecond;
  }

  void ExpectChildScreenTimeMilliseconds(int64_t duration) {
    pref_service()->CommitPendingWrite(
        base::OnceClosure(),
        base::BindOnce(
            [](int64_t duration, PrefService* profile_pref_service_) {
              EXPECT_EQ(duration, profile_pref_service_->GetInteger(
                                      prefs::kChildScreenTimeMilliseconds));
            },
            duration, pref_service()));
  }

  void ExpectLastChildScreenTimeReset(Time time) {
    pref_service()->CommitPendingWrite(
        base::OnceClosure(),
        base::BindOnce(
            [](Time time, PrefService* profile_pref_service_) {
              EXPECT_EQ(time, profile_pref_service_->GetTime(
                                  prefs::kLastChildScreenTimeReset));
            },
            time, pref_service()));
  }

  void FastForwardTo(Time time) {
    base::TimeDelta forward_by = time - Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);
  }

  Profile* testing_profile() { return testing_profile_.get(); }
  PrefService* pref_service() { return testing_profile_->GetPrefs(); }

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::FakeOwnerSettingsService owner_settings_service_{
      scoped_testing_cros_settings_.device_settings(), nullptr};
  // local_state_ should be destructed after TestingProfile.
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> testing_profile_;
  user_manager::ScopedUserManager user_manager_enabler_;
  em::ChildStatusReportRequest child_status_;
  std::unique_ptr<TestingChildStatusCollector> status_collector_;
  base::ScopedPathOverride user_data_dir_override_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
};

TEST_F(ChildStatusCollectorTest, ReportingBootMode) {
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kDevSwitchBootKey, ash::system::kDevSwitchBootValueVerified);

  GetStatus();

  EXPECT_TRUE(child_status_.has_boot_mode());
  EXPECT_EQ("Verified", child_status_.boot_mode());
}

TEST_F(ChildStatusCollectorTest, ReportingArcStatus) {
  RestartStatusCollector(
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo));
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();

  EXPECT_EQ(kArcStatus, child_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo, child_status_.android_status().droid_guard_info());
  EXPECT_EQ(kFakeDmToken, child_status_.user_dm_token());
}

TEST_F(ChildStatusCollectorTest, ReportingPartialVersionInfo) {
  GetStatus();

  EXPECT_TRUE(child_status_.has_os_version());
}

TEST_F(ChildStatusCollectorTest, TimeZoneReporting) {
  const std::string timezone = base::UTF16ToUTF8(
      ash::system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());

  GetStatus();

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

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ReportingActivityTimesSleepTransitions) {
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

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ActivityKeptInPref) {
  EXPECT_THAT(pref_service()->GetDict(prefs::kUserActivityTimes), IsEmpty());
  task_environment_.AdvanceClock(kHour);

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
  EXPECT_THAT(pref_service()->GetDict(prefs::kUserActivityTimes),
              Not(IsEmpty()));

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));
  // Avoid resetting to test accumulating screen time.
  pref_service()->SetTime(prefs::kLastChildScreenTimeReset, Time::Now());
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

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

  EXPECT_EQ(1, child_status_.screen_time_span_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(child_status_));
  ExpectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds());
  // Nothing should be written to local state, because it is only used for
  // enterprise reporting.
}

TEST_F(ChildStatusCollectorTest, BeforeDayStart) {
  RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus), kSixAm);
  // TaskEnvironment can't go backwards in time, so fast forward to 04:00 AM on
  // the next day.
  Time initial_time =
      Time::Now().LocalMidnight() + base::Days(1) + base::Hours(4);
  FastForwardTo(initial_time);
  EXPECT_THAT(pref_service()->GetDict(prefs::kUserActivityTimes), IsEmpty());

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
  // TaskEnvironment can't go backwards in time, so fast forward to 11:45:45 PM
  // on the next day.
  Time start_time =
      Time::Now().LocalMidnight() + base::Days(1) - base::Seconds(15);
  FastForwardTo(start_time);
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));
  GetStatus();

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
            base::Time::kMillisecondsPerDay);
  EXPECT_EQ(timespan1period.end_timestamp() - timespan1period.start_timestamp(),
            base::Time::kMillisecondsPerDay);
  ExpectChildScreenTimeMilliseconds(0.5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ClockChanged) {
  DeviceStateTransitions test_states[1] = {
      DeviceStateTransitions::kEnterSessionActive};
  Time initial_time;
  // Test daylight savings time (spring forward).
  ASSERT_TRUE(Time::FromString("30 Mar 2020 1:00AM PST", &initial_time));
  FastForwardTo(initial_time);
  SimulateStateChanges(test_states, 1);

  // Simulate a real DST clock change.
  task_environment_.AdvanceClock(kHour);
  test_states[0] = DeviceStateTransitions::kLeaveSessionActive;
  SimulateStateChanges(test_states, 1);

  GetStatus();

  ASSERT_EQ(1, child_status_.screen_time_span_size());
  ExpectChildScreenTimeMilliseconds(2 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ReportingAppActivity) {
  // Nothing reported yet.
  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
  status_collector_->OnSubmittedSuccessfully();

  // Report activity for two different apps.
  const ash::app_time::AppId app1(apps::AppType::kWeb, "app1");
  const ash::app_time::AppId app2(apps::AppType::kChromeApp, "app2");
  const Time start_time = Time::Now();
  const base::TimeDelta app1_interval = base::Minutes(1);
  const base::TimeDelta app2_interval = base::Minutes(2);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  GetStatus();
  EXPECT_EQ(2, child_status_.app_activity_size());

  for (const auto& app_activity : child_status_.app_activity()) {
    if (app_activity.app_info().app_id() == app1.app_id()) {
      EXPECT_EQ(em::App::WEB, app_activity.app_info().app_type());
      EXPECT_EQ(0, app_activity.app_info().additional_app_id_size());
      EXPECT_EQ(em::AppActivity::DEFAULT, app_activity.app_state());
      EXPECT_EQ(3, app_activity.active_time_periods_size());
      Time start = start_time;
      for (const auto& active_period : app_activity.active_time_periods()) {
        EXPECT_EQ(start.InMillisecondsSinceUnixEpoch(),
                  active_period.start_timestamp());
        const Time end = start + app1_interval;
        EXPECT_EQ(end.InMillisecondsSinceUnixEpoch(),
                  active_period.end_timestamp());
        start = end + app2_interval;
      }
      continue;
    }
    if (app_activity.app_info().app_id() == app2.app_id()) {
      EXPECT_EQ(em::App::EXTENSION, app_activity.app_info().app_type());
      EXPECT_EQ(0, app_activity.app_info().additional_app_id_size());
      EXPECT_EQ(em::AppActivity::DEFAULT, app_activity.app_state());
      EXPECT_EQ(2, app_activity.active_time_periods_size());
      Time start = start_time + app1_interval;
      for (const auto& active_period : app_activity.active_time_periods()) {
        EXPECT_EQ(start.InMillisecondsSinceUnixEpoch(),
                  active_period.start_timestamp());
        const Time end = start + app2_interval;
        EXPECT_EQ(end.InMillisecondsSinceUnixEpoch(),
                  active_period.end_timestamp());
        start = end + app1_interval;
      }
      continue;
    }
  }

  // After successful report submission 'old' data should be cleared.
  status_collector_->OnSubmittedSuccessfully();
  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
}

TEST_F(ChildStatusCollectorTest, ReportingAppActivityNoReport) {
  // Nothing reported yet.
  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
  status_collector_->OnSubmittedSuccessfully();

  const ash::app_time::AppId app1(apps::AppType::kWeb, "app1");
  const ash::app_time::AppId app2(apps::AppType::kChromeApp, "app2");
  const base::TimeDelta app1_interval = base::Minutes(1);
  const base::TimeDelta app2_interval = base::Minutes(2);

  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  {
    ash::app_time::AppTimeLimitsPolicyBuilder builder;
    builder.SetAppActivityReportingEnabled(/* enabled */ false);
    testing_profile()->GetPrefs()->SetDict(prefs::kPerAppTimeLimitsPolicy,
                                           builder.value().Clone());
  }

  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
}

TEST_F(ChildStatusCollectorTest, ReportingAppActivityMetrics) {
  base::HistogramTester histogram_tester;

  // Nothing reported yet.
  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
  status_collector_->OnSubmittedSuccessfully();

  histogram_tester.ExpectTotalCount(
      ChildStatusCollector::GetReportSizeHistogramNameForTest(),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      ChildStatusCollector::GetTimeSinceLastReportHistogramNameForTest(),
      /*expected_count=*/0);

  // Report activity for two different apps.
  const ash::app_time::AppId app1(apps::AppType::kWeb, "app1");
  const ash::app_time::AppId app2(apps::AppType::kChromeApp, "app2");
  const base::TimeDelta app1_interval = base::Seconds(1);
  const base::TimeDelta app2_interval = base::Seconds(2);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  GetStatus();
  status_collector_->OnSubmittedSuccessfully();
  EXPECT_EQ(2, child_status_.app_activity_size());

  // The amount of data is less than one KB, so that rounds down to zero.
  histogram_tester.ExpectUniqueSample(
      ChildStatusCollector::GetReportSizeHistogramNameForTest(), /*sample=*/0,
      /*expected_count=*/1);
  // There was no previous report, so the time elapsed since the last report is
  // not applicable.
  histogram_tester.ExpectTotalCount(
      ChildStatusCollector::GetTimeSinceLastReportHistogramNameForTest(),
      /*expected_count=*/0);

  // Generate a much larger report. The report size needs to exceed 1000KB to
  // reach the next bucket size in the histogram. Also fast forwards by 2000
  // minutes.
  for (int i = 0; i < 40000; i++) {
    SimulateAppActivity(app1, app1_interval, /*should_run_tasks=*/false);
    SimulateAppActivity(app2, app2_interval, /*should_run_tasks=*/false);
  }

  GetStatus();
  status_collector_->OnSubmittedSuccessfully();
  EXPECT_EQ(2, child_status_.app_activity_size());

  histogram_tester.ExpectBucketCount(
      ChildStatusCollector::GetReportSizeHistogramNameForTest(),
      /*sample=*/1250 /*KB*/,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      ChildStatusCollector::GetReportSizeHistogramNameForTest(),
      /*expected_count=*/2);
  // 2000 minutes (33 hours) have elapsed since the last report.
  histogram_tester.ExpectUniqueSample(
      ChildStatusCollector::GetTimeSinceLastReportHistogramNameForTest(),
      /*sample=*/2000, /*expected_count=*/1);
}

}  // namespace policy
