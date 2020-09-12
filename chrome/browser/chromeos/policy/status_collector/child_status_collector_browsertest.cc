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
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/status_collector/child_status_collector.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
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
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
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

constexpr int kIdlePollIntervalSeconds = 30;

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
      Profile* profile,
      chromeos::system::StatisticsProvider* provider,
      const policy::StatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      TimeDelta activity_day_start,
      base::SimpleTestClock* clock)
      : policy::ChildStatusCollector(pref_service,
                                     profile,
                                     provider,
                                     android_status_fetcher,
                                     activity_day_start) {
    clock_ = clock;
  }

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
    const policy::ChildStatusCollector::AndroidStatusReceiver& receiver,
    const std::string& status,
    const std::string& droid_guard_info) {
  receiver.Run(status, droid_guard_info);
}

bool GetEmptyAndroidStatus(
    const policy::StatusCollector::AndroidStatusReceiver& receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, receiver, "", ""));
  return true;
}

bool GetFakeAndroidStatus(
    const std::string& status,
    const std::string& droid_guard_info,
    const policy::StatusCollector::AndroidStatusReceiver& receiver) {
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
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {features::kPerAppTimeLimits,
                                features::kAppActivityReporting},
        /* disabled_features */ {});

    RestartStatusCollector(base::BindRepeating(&GetEmptyAndroidStatus));

    // Disable network interface reporting since it requires additional setup.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kReportDeviceNetworkInterfaces, false);

    // Mock clock in task environment is set to Unix Epoch, advance it to avoid
    // using times from before Unix Epoch in some tests.
    task_environment_.AdvanceClock(base::TimeDelta::FromDays(365));
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
              base::TimeDelta::FromSeconds(kIdlePollIntervalSeconds));
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
      test_clock_.Advance(TimeDelta::FromSeconds(kIdlePollIntervalSeconds));
      status_collector_->UpdateUsageTime();
    }
  }

  void SimulateAppActivity(const chromeos::app_time::AppId& app_id,
                           base::TimeDelta duration) {
    chromeos::ChildUserService::TestApi child_user_service =
        chromeos::ChildUserService::TestApi(
            chromeos::ChildUserServiceFactory::GetForBrowserContext(
                testing_profile_.get()));
    EXPECT_TRUE(child_user_service.app_time_controller());

    chromeos::app_time::AppActivityRegistry* app_registry =
        chromeos::app_time::AppTimeController::TestApi(
            child_user_service.app_time_controller())
            .app_registry();
    app_registry->OnAppInstalled(app_id);

    // Window instance is irrelevant for tests here.
    app_registry->OnAppActive(app_id, nullptr /* window */, test_clock_.Now());
    task_environment_.FastForwardBy(duration);
    test_clock_.Advance(duration);
    app_registry->OnAppInactive(app_id, nullptr /* window */,
                                test_clock_.Now());
  }

  virtual void RestartStatusCollector(
      const policy::StatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      const TimeDelta activity_day_start = kMidnight) {
    // Set the baseline time to a fixed value (1 hour after day start) to
    // prevent test flakiness due to a single activity period spanning two days.
    test_clock_.SetNow(Time::Now().LocalMidnight() + activity_day_start +
                       kHour);
    status_collector_ = std::make_unique<TestingChildStatusCollector>(
        &profile_pref_service_, testing_profile_.get(),
        &fake_statistics_provider_, android_status_fetcher, activity_day_start,
        &test_clock_);
  }

  void GetStatus() {
    run_loop_.reset(new base::RunLoop());
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
                                                 test_clock_.Now());
  }

  // Convenience method.
  int64_t ActivePeriodMilliseconds() { return kIdlePollIntervalSeconds * 1000; }

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

  Profile* testing_profile() { return testing_profile_.get(); }

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::FakeOwnerSettingsService owner_settings_service_{
      scoped_testing_cros_settings_.device_settings(), nullptr};
  // local_state_ should be destructed after TestingProfile.
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> testing_profile_;
  chromeos::MockUserManager* const user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  em::ChildStatusReportRequest child_status_;
  TestingPrefServiceSimple profile_pref_service_;
  std::unique_ptr<TestingChildStatusCollector> status_collector_;
  base::ScopedPathOverride user_data_dir_override_;
  chromeos::FakeUpdateEngineClient* const update_engine_client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::SimpleTestClock test_clock_;

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
  const std::string timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());

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
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());
  base::Time initial_time = base::Time::Now() + kHour;
  test_clock_.SetNow(initial_time);

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
  test_clock_.SetNow(initial_time);
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
  // 04:00 AM
  Time initial_time = Time::Now().LocalMidnight() + TimeDelta::FromHours(4);
  test_clock_.SetNow(initial_time);
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
  test_clock_.SetNow(Time::Now().LocalMidnight() - TimeDelta::FromSeconds(15));
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
            kMillisecondsPerDay);
  EXPECT_EQ(timespan1period.end_timestamp() - timespan1period.start_timestamp(),
            kMillisecondsPerDay);
  ExpectChildScreenTimeMilliseconds(0.5 * ActivePeriodMilliseconds());
}

TEST_F(ChildStatusCollectorTest, ClockChanged) {
  DeviceStateTransitions test_states[1] = {
      DeviceStateTransitions::kEnterSessionActive};
  base::Time initial_time;
  ASSERT_TRUE(base::Time::FromString("30 Mar 2020 1:00AM PST", &initial_time));
  test_clock_.SetNow(initial_time);
  SimulateStateChanges(test_states, 1);

  // Simulate a real DST clock change.
  base::Time clock_change_time;
  ASSERT_TRUE(
      base::Time::FromString("30 Mar 2020 2:00AM PDT", &clock_change_time));
  test_clock_.SetNow(clock_change_time);
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
  const chromeos::app_time::AppId app1(apps::mojom::AppType::kWeb, "app1");
  const chromeos::app_time::AppId app2(apps::mojom::AppType::kExtension,
                                       "app2");
  const base::Time start_time = base::Time::Now();
  test_clock_.SetNow(start_time);
  const base::TimeDelta app1_interval = base::TimeDelta::FromMinutes(1);
  const base::TimeDelta app2_interval = base::TimeDelta::FromMinutes(2);
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
      base::Time start = start_time;
      for (const auto& active_period : app_activity.active_time_periods()) {
        EXPECT_EQ(start.ToJavaTime(), active_period.start_timestamp());
        const base::Time end = start + app1_interval;
        EXPECT_EQ(end.ToJavaTime(), active_period.end_timestamp());
        start = end + app2_interval;
      }
      continue;
    }
    if (app_activity.app_info().app_id() == app2.app_id()) {
      EXPECT_EQ(em::App::EXTENSION, app_activity.app_info().app_type());
      EXPECT_EQ(0, app_activity.app_info().additional_app_id_size());
      EXPECT_EQ(em::AppActivity::DEFAULT, app_activity.app_state());
      EXPECT_EQ(2, app_activity.active_time_periods_size());
      base::Time start = start_time + app1_interval;
      for (const auto& active_period : app_activity.active_time_periods()) {
        EXPECT_EQ(start.ToJavaTime(), active_period.start_timestamp());
        const base::Time end = start + app2_interval;
        EXPECT_EQ(end.ToJavaTime(), active_period.end_timestamp());
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

  const chromeos::app_time::AppId app1(apps::mojom::AppType::kWeb, "app1");
  const chromeos::app_time::AppId app2(apps::mojom::AppType::kExtension,
                                       "app2");
  const base::TimeDelta app1_interval = base::TimeDelta::FromMinutes(1);
  const base::TimeDelta app2_interval = base::TimeDelta::FromMinutes(2);

  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  {
    chromeos::app_time::AppTimeLimitsPolicyBuilder builder;
    builder.SetAppActivityReportingEnabled(/* enabled */ false);
    DictionaryPrefUpdate update(testing_profile()->GetPrefs(),
                                prefs::kPerAppTimeLimitsPolicy);
    base::Value* value = update.Get();
    *value = builder.value().Clone();
  }

  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);
  SimulateAppActivity(app2, app2_interval);
  SimulateAppActivity(app1, app1_interval);

  GetStatus();
  EXPECT_EQ(0, child_status_.app_activity_size());
}

}  // namespace policy
