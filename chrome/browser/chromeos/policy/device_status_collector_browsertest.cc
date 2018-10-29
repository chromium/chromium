// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/mount_points.h"
#include "storage/common/fileapi/file_system_mount_option.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::Return;
using ::testing::ReturnRef;
using base::Time;
using base::TimeDelta;
using chromeos::disks::DiskMountManager;

namespace em = enterprise_management;

namespace {

// Time delta representing midnight 00:00.
constexpr TimeDelta kMidnight;

// Time delta representing 1 hour time interval.
constexpr TimeDelta kHour = TimeDelta::FromHours(1);

const int64_t kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;
const char kKioskAccountId[] = "kiosk_user@localhost";
const char kArcKioskAccountId[] = "arc_kiosk_user@localhost";
const char kKioskAppId[] = "kiosk_app_id";
const char kArcKioskPackageName[] = "com.test.kioskapp";
const char kExternalMountPoint[] = "/a/b/c";
const char kPublicAccountId[] = "public_user@localhost";
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
const char kShillFakeProfilePath[] = "/profile/user1/shill";
const char kShillFakeUserhash[] = "user1";

class TestingDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  TestingDeviceStatusCollector(
      PrefService* pref_service,
      chromeos::system::StatisticsProvider* provider,
      const policy::DeviceStatusCollector::VolumeInfoFetcher&
          volume_info_fetcher,
      const policy::DeviceStatusCollector::CPUStatisticsFetcher& cpu_fetcher,
      const policy::DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher,
      const policy::DeviceStatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      const policy::DeviceStatusCollector::TpmStatusFetcher& tpm_status_fetcher,
      TimeDelta activity_day_start,
      bool is_enterprise_device)
      : policy::DeviceStatusCollector(pref_service,
                                      provider,
                                      volume_info_fetcher,
                                      cpu_fetcher,
                                      cpu_temp_fetcher,
                                      android_status_fetcher,
                                      tpm_status_fetcher,
                                      activity_day_start,
                                      is_enterprise_device) {
    // Set the baseline time to a fixed value (1 hour after day start) to
    // prevent test flakiness due to a single activity period spanning two days.
    SetBaselineTime(Time::Now().LocalMidnight() + activity_day_start + kHour);
  }

  void UpdateUsageTime() { UpdateChildUsageTime(); }

  void Simulate(ui::IdleState* states, int len) {
    for (int i = 0; i < len; i++)
      IdleStateCallback(states[i]);
  }

  void set_max_stored_past_activity_interval(TimeDelta value) {
    max_stored_past_activity_interval_ = value;
  }

  void set_max_stored_future_activity_interval(TimeDelta value) {
    max_stored_future_activity_interval_ = value;
  }

  // Reset the baseline time.
  void SetBaselineTime(Time time) {
    baseline_time_ = time;
    baseline_offset_periods_ = 0;
  }

  void set_kiosk_account(std::unique_ptr<policy::DeviceLocalAccount> account) {
    kiosk_account_ = std::move(account);
  }

  std::unique_ptr<policy::DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo()
      override {
    if (kiosk_account_)
      return std::make_unique<policy::DeviceLocalAccount>(*kiosk_account_);
    return std::unique_ptr<policy::DeviceLocalAccount>();
  }

  std::string GetAppVersion(const std::string& app_id) override {
    // Just return the app_id as the version - this makes it easy for tests
    // to confirm that the correct app's version was requested.
    return app_id;
  }

  std::string GetDMTokenForProfile(Profile* profile) override {
    // Return the profile user name (passed to CreateTestingProfile) to make it
    // easy to confirm that the correct profile's DMToken was requested.
    return profile->GetProfileUserName();
  }

  void RefreshSampleResourceUsage() {
    SampleResourceUsage();
    content::RunAllTasksUntilIdle();
  }

 protected:
  void CheckIdleState() override {
    // This should never be called in testing, as it results in a dbus call.
    ADD_FAILURE();
  }

  // Each time this is called, returns a time that is a fixed increment
  // later than the previous time.
  Time GetCurrentTime() override {
    int poll_interval = policy::DeviceStatusCollector::kIdlePollIntervalSeconds;
    return baseline_time_ +
           TimeDelta::FromSeconds(poll_interval * baseline_offset_periods_++);
  }

 private:
  // Baseline time for the fake times returned from GetCurrentTime().
  Time baseline_time_;

  // The number of simulated periods since the baseline time.
  int baseline_offset_periods_;

  std::unique_ptr<policy::DeviceLocalAccount> kiosk_account_;
};

// Return the total number of active milliseconds contained in a device
// status report.
int64_t GetActiveMilliseconds(const em::DeviceStatusReportRequest& status) {
  int64_t active_milliseconds = 0;
  for (int i = 0; i < status.active_period_size(); i++) {
    active_milliseconds += status.active_period(i).active_duration();
  }
  return active_milliseconds;
}

// Mock CPUStatisticsFetcher used to return an empty set of statistics.
std::string GetEmptyCPUStatistics() {
  return std::string();
}

std::string GetFakeCPUStatistics(const std::string& fake) {
  return fake;
}

// Mock VolumeInfoFetcher used to return empty VolumeInfo, to avoid warnings
// and test slowdowns from trying to fetch information about non-existent
// volumes.
std::vector<em::VolumeInfo> GetEmptyVolumeInfo(
    const std::vector<std::string>& mount_points) {
  return std::vector<em::VolumeInfo>();
}

std::vector<em::VolumeInfo> GetFakeVolumeInfo(
    const std::vector<em::VolumeInfo>& volume_info,
    const std::vector<std::string>& mount_points) {
  EXPECT_EQ(volume_info.size(), mount_points.size());
  // Make sure there's a matching mount point for every volume info.
  for (const em::VolumeInfo& info : volume_info) {
    bool found = false;
    for (const std::string& mount_point : mount_points) {
      if (info.volume_id() == mount_point) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Could not find matching mount point for "
                       << info.volume_id();
  }
  return volume_info;
}

std::vector<em::CPUTempInfo> GetEmptyCPUTempInfo() {
  return std::vector<em::CPUTempInfo>();
}

std::vector<em::CPUTempInfo> GetFakeCPUTempInfo(
    const std::vector<em::CPUTempInfo>& cpu_temp_info) {
  return cpu_temp_info;
}

void CallAndroidStatusReceiver(
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver,
    const std::string& status,
    const std::string& droid_guard_info) {
  receiver.Run(status, droid_guard_info);
}

bool GetEmptyAndroidStatus(
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, receiver, "", ""));
  return true;
}

bool GetFakeAndroidStatus(
    const std::string& status,
    const std::string& droid_guard_info,
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver) {
  // Post it to the thread because this call is expected to be asynchronous.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CallAndroidStatusReceiver, receiver, status,
                                droid_guard_info));
  return true;
}

void GetEmptyTpmStatus(
    policy::DeviceStatusCollector::TpmStatusReceiver receiver) {
  std::move(receiver).Run(policy::TpmStatusInfo());
}

void GetFakeTpmStatus(
    const policy::TpmStatusInfo& tpm_status_info,
    policy::DeviceStatusCollector::TpmStatusReceiver receiver) {
  std::move(receiver).Run(tpm_status_info);
}

}  // namespace

namespace policy {

// Though it is a unit test, this test is linked with browser_tests so that it
// runs in a separate process. The intention is to avoid overriding the timezone
// environment variable for other tests.
class DeviceStatusCollectorTest : public testing::Test {
 public:
  DeviceStatusCollectorTest()
      : user_manager_(new chromeos::MockUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_)),
        got_session_status_(false),
        fake_kiosk_device_local_account_(
            policy::DeviceLocalAccount::TYPE_KIOSK_APP,
            kKioskAccountId,
            kKioskAppId,
            std::string() /* kiosk_app_update_url */),
        fake_arc_kiosk_app_basic_info_(kArcKioskPackageName,
                                       std::string() /* class_name */,
                                       std::string() /* action */,
                                       std::string() /* display_name */),
        fake_arc_kiosk_device_local_account_(fake_arc_kiosk_app_basic_info_,
                                             kArcKioskAccountId),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        update_engine_client_(new chromeos::FakeUpdateEngineClient) {
    settings_helper_.InstallAttributes()->SetCloudManaged("managed.com",
                                                          "device_id");
    EXPECT_CALL(*user_manager_, Shutdown()).Times(1);

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

    // Initialize our mock mounted disk volumes.
    std::unique_ptr<chromeos::disks::MockDiskMountManager>
        mock_disk_mount_manager =
            std::make_unique<chromeos::disks::MockDiskMountManager>();
    AddMountPoint("/mount/volume1");
    AddMountPoint("/mount/volume2");
    EXPECT_CALL(*mock_disk_mount_manager, mount_points())
        .WillRepeatedly(ReturnRef(mount_point_map_));

    // Setup a fake file system that should show up in mount points.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        "c", storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), base::FilePath(kExternalMountPoint));

    // Just verify that we are properly setting the mount points.
    std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
    storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
        &external_mount_points);
    EXPECT_FALSE(external_mount_points.empty());

    // DiskMountManager takes ownership of the MockDiskMountManager.
    DiskMountManager::InitializeForTesting(mock_disk_mount_manager.release());
    TestingDeviceStatusCollector::RegisterPrefs(local_state_.registry());
    TestingDeviceStatusCollector::RegisterProfilePrefs(
        profile_pref_service_.registry());

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(nullptr);
    owner_settings_service_->set_ignore_profile_creation_notification(true);

    // Set up a fake local state for KioskAppManager.
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    chromeos::KioskAppManager::RegisterPrefs(local_state_.registry());

    // Use FakeUpdateEngineClient.
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        base::WrapUnique<chromeos::UpdateEngineClient>(update_engine_client_));

    chromeos::CrasAudioHandler::InitializeForTesting();
    chromeos::LoginState::Initialize();

    fake_power_manager_client_ = new chromeos::FakePowerManagerClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        base::WrapUnique(fake_power_manager_client_));
  }

  ~DeviceStatusCollectorTest() override {
    chromeos::LoginState::Shutdown();
    chromeos::CrasAudioHandler::Shutdown();
    chromeos::KioskAppManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    // Finish pending tasks.
    content::RunAllTasksUntilIdle();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    DiskMountManager::Shutdown();
  }

  void SetUp() override {
    RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                           base::BindRepeating(&GetEmptyCPUStatistics),
                           base::BindRepeating(&GetEmptyCPUTempInfo),
                           base::BindRepeating(&GetEmptyAndroidStatus),
                           base::BindRepeating(&GetEmptyTpmStatus));

    // Disable network interface reporting since it requires additional setup.
    settings_helper_.SetBoolean(chromeos::kReportDeviceNetworkInterfaces,
                                false);
  }

  void TearDown() override {
    settings_helper_.RestoreRealDeviceSettingsProvider();
  }

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
          fake_power_manager_client_->SendScreenIdleStateChanged(state);
        } break;
        case DeviceStateTransitions::kLeaveIdleState: {
          power_manager::ScreenIdleState state;
          state.set_off(false);
          fake_power_manager_client_->SendScreenIdleStateChanged(state);
        } break;
        case DeviceStateTransitions::kEnterSleep:
          fake_power_manager_client_->SendSuspendImminent(
              power_manager::SuspendImminent_Reason_LID_CLOSED);
          break;
        case DeviceStateTransitions::kLeaveSleep:
          fake_power_manager_client_->SendSuspendDone(
              base::TimeDelta::FromSeconds(
                  policy::DeviceStatusCollector::kIdlePollIntervalSeconds));
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

  void AddMountPoint(const std::string& mount_point) {
    mount_point_map_.insert(DiskMountManager::MountPointMap::value_type(
        mount_point, DiskMountManager::MountPointInfo(
                         mount_point, mount_point, chromeos::MOUNT_TYPE_DEVICE,
                         chromeos::disks::MOUNT_CONDITION_NONE)));
  }

  virtual void RestartStatusCollector(
      const policy::DeviceStatusCollector::VolumeInfoFetcher& volume_info,
      const policy::DeviceStatusCollector::CPUStatisticsFetcher& cpu_stats,
      const policy::DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher,
      const policy::DeviceStatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      const policy::DeviceStatusCollector::TpmStatusFetcher&
          tpm_status_fetcher) {
    std::vector<em::VolumeInfo> expected_volume_info;
    status_collector_.reset(new TestingDeviceStatusCollector(
        &local_state_, &fake_statistics_provider_, volume_info, cpu_stats,
        cpu_temp_fetcher, android_status_fetcher, tpm_status_fetcher, kMidnight,
        true /* is_enterprise_device */));
  }

  void GetStatus() {
    device_status_.Clear();
    session_status_.Clear();
    got_session_status_ = false;
    run_loop_.reset(new base::RunLoop());
    status_collector_->GetDeviceAndSessionStatusAsync(base::BindRepeating(
        &DeviceStatusCollectorTest::OnStatusReceived, base::Unretained(this)));
    run_loop_->Run();
    run_loop_.reset();
  }

  void OnStatusReceived(
      std::unique_ptr<em::DeviceStatusReportRequest> device_status,
      std::unique_ptr<em::SessionStatusReportRequest> session_status) {
    if (device_status)
      device_status_ = *device_status;
    got_session_status_ = session_status != nullptr;
    if (got_session_status_)
      session_status_ = *session_status;
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

  void MockRegularUserWithAffiliation(const AccountId& account_id,
                                      bool is_affiliated) {
    MockUserWithTypeAndAffiliation(account_id, user_manager::USER_TYPE_REGULAR,
                                   is_affiliated);
  }

  void MockChildUser(const AccountId& account_id) {
    MockUserWithTypeAndAffiliation(account_id, user_manager::USER_TYPE_CHILD,
                                   false);
    EXPECT_CALL(*user_manager_, IsLoggedInAsChildUser())
        .WillRepeatedly(Return(true));
  }

  void MockRunningKioskApp(const DeviceLocalAccount& account, bool arc_kiosk) {
    std::vector<DeviceLocalAccount> accounts;
    accounts.push_back(account);
    user_manager::User* user = user_manager_->CreateKioskAppUser(
        AccountId::FromUserEmail(account.user_id));
    if (arc_kiosk) {
      EXPECT_CALL(*user_manager_, IsLoggedInAsArcKioskApp())
          .WillRepeatedly(Return(true));
    } else {
      EXPECT_CALL(*user_manager_, IsLoggedInAsKioskApp())
          .WillRepeatedly(Return(true));
    }

    testing_profile_ = std::make_unique<TestingProfile>();
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());

    SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);
  }

  void MockPlatformVersion(const std::string& platform_version) {
    const std::string lsb_release = base::StringPrintf(
        "CHROMEOS_RELEASE_VERSION=%s", platform_version.c_str());
    base::SysInfo::SetChromeOSVersionInfoForTest(lsb_release,
                                                 base::Time::Now());
  }

  void MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      const DeviceLocalAccount& auto_launch_app_account,
      const std::string& required_platform_version) {
    chromeos::KioskAppManager* manager = chromeos::KioskAppManager::Get();
    manager->AddAppForTest(
        auto_launch_app_account.kiosk_app_id,
        AccountId::FromUserEmail(auto_launch_app_account.user_id),
        GURL("http://cws/"),  // Dummy URL to avoid setup ExtensionsClient.
        required_platform_version);
    manager->SetEnableAutoLaunch(true);

    std::vector<DeviceLocalAccount> accounts;
    accounts.push_back(auto_launch_app_account);
    SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);

    owner_settings_service_->SetString(
        chromeos::kAccountsPrefDeviceLocalAccountAutoLoginId,
        auto_launch_app_account.account_id);

    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(required_platform_version,
              manager->GetAutoLaunchAppRequiredPlatformVersion());
  }

  void MockAutoLaunchArcKioskApp(
      const DeviceLocalAccount& auto_launch_app_account) {
    arc_kiosk_app_manager_.reset(new chromeos::ArcKioskAppManager());
    arc_kiosk_app_manager_->AddAutoLaunchAppForTest(
        auto_launch_app_account.arc_kiosk_app_info.package_name(),
        auto_launch_app_account.arc_kiosk_app_info,
        AccountId::FromUserEmail(auto_launch_app_account.user_id));

    std::vector<DeviceLocalAccount> accounts;
    accounts.push_back(auto_launch_app_account);
    SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);

    owner_settings_service_->SetString(
        chromeos::kAccountsPrefDeviceLocalAccountAutoLoginId,
        auto_launch_app_account.account_id);

    base::RunLoop().RunUntilIdle();
  }

  // Convenience method.
  int64_t ActivePeriodMilliseconds() {
    return policy::DeviceStatusCollector::kIdlePollIntervalSeconds * 1000;
  }

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DiskMountManager::MountPointMap mount_point_map_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  // Only set after MockRunningKioskApp or MockTODO was called.
  std::unique_ptr<chromeos::FakeOwnerSettingsService> owner_settings_service_;
  // Only set after MockRunningKioskApp was called.
  std::unique_ptr<TestingProfile> testing_profile_;
  // Only set after MockAutoLaunchArcKioskApp was called.
  std::unique_ptr<chromeos::ArcKioskAppManager> arc_kiosk_app_manager_;
  chromeos::MockUserManager* const user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  em::DeviceStatusReportRequest device_status_;
  em::SessionStatusReportRequest session_status_;
  bool got_session_status_;
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple profile_pref_service_;
  std::unique_ptr<TestingDeviceStatusCollector> status_collector_;
  const policy::DeviceLocalAccount fake_kiosk_device_local_account_;
  const policy::ArcKioskAppBasicInfo fake_arc_kiosk_app_basic_info_;
  const policy::DeviceLocalAccount fake_arc_kiosk_device_local_account_;
  base::ScopedPathOverride user_data_dir_override_;
  chromeos::FakeUpdateEngineClient* const update_engine_client_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // Owned by chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* fake_power_manager_client_;

  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
};

TEST_F(DeviceStatusCollectorTest, AllIdle) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE
  };
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Test reporting with no data.
  GetStatus();
  EXPECT_EQ(0, device_status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));

  // Test reporting with a single idle sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(0, device_status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));

  // Test reporting with multiple consecutive idle samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(0, device_status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, AllActive) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE
  };
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Test a single active sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  device_status_.clear_active_period();  // Clear the result protobuf.

  // Test multiple consecutive active samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, MixedStates) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_ACTIVE
  };
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

// For kiosks report total uptime instead of only active periods.
TEST_F(DeviceStatusCollectorTest, MixedStatesForKiosk) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE,
  };
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP);
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(6 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

// For Arc kiosks report total uptime instead of only active periods.
TEST_F(DeviceStatusCollectorTest, MixedStatesForArcKiosk) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
  };
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP);
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, StateKeptInPref) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE
  };
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                         base::BindRepeating(&GetEmptyCPUStatistics),
                         base::BindRepeating(&GetEmptyCPUTempInfo),
                         base::BindRepeating(&GetEmptyAndroidStatus),
                         base::BindRepeating(&GetEmptyTpmStatus));
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));

  GetStatus();
  EXPECT_EQ(6 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityNotWrittenToProfilePref) {
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());

  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));

  // Nothing should be written to profile pref service, because it is only used
  // for consumer reporting.
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());
}

TEST_F(DeviceStatusCollectorTest, MaxStoredPeriods) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_IDLE
  };
  const int kMaxDays = 10;

  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->set_max_stored_past_activity_interval(
      TimeDelta::FromDays(kMaxDays - 1));
  status_collector_->set_max_stored_future_activity_interval(
      TimeDelta::FromDays(1));
  Time baseline = Time::Now().LocalMidnight();

  // Simulate 12 active periods.
  for (int i = 0; i < kMaxDays + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(ui::IdleState));
    // Advance the simulated clock by a day.
    baseline += TimeDelta::FromDays(1);
    status_collector_->SetBaselineTime(baseline);
  }

  // Check that we don't exceed the max number of periods.
  GetStatus();
  EXPECT_EQ(kMaxDays - 1, device_status_.active_period_size());

  // Simulate some future times.
  for (int i = 0; i < kMaxDays + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(ui::IdleState));
    // Advance the simulated clock by a day.
    baseline += TimeDelta::FromDays(1);
    status_collector_->SetBaselineTime(baseline);
  }
  // Set the clock back so the previous simulated times are in the future.
  baseline -= TimeDelta::FromDays(20);
  status_collector_->SetBaselineTime(baseline);

  // Collect one more data point to trigger pruning.
  status_collector_->Simulate(test_states, 1);

  // Check that we don't exceed the max number of periods.
  device_status_.clear_active_period();
  GetStatus();
  EXPECT_LT(device_status_.active_period_size(), kMaxDays);
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesEnabledByDefault) {
  // Device activity times should be reported by default.
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE
  };
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesOff) {
  // Device activity times should not be reported if explicitly disabled.
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, false);

  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE
  };
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(0, device_status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityCrossingMidnight) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE
  };
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Set the baseline time to 10 seconds after midnight.
  status_collector_->SetBaselineTime(
      Time::Now().LocalMidnight() + TimeDelta::FromSeconds(10));

  status_collector_->Simulate(test_states, 1);
  GetStatus();
  ASSERT_EQ(2, device_status_.active_period_size());

  em::ActiveTimePeriod period0 = device_status_.active_period(0);
  em::ActiveTimePeriod period1 = device_status_.active_period(1);
  EXPECT_EQ(ActivePeriodMilliseconds() - 10000, period0.active_duration());
  EXPECT_EQ(10000, period1.active_duration());

  em::TimePeriod time_period0 = period0.time_period();
  em::TimePeriod time_period1 = period1.time_period();

  EXPECT_EQ(time_period0.end_timestamp(), time_period1.start_timestamp());

  // Ensure that the start and end times for the period are a day apart.
  EXPECT_EQ(time_period0.end_timestamp() - time_period0.start_timestamp(),
            kMillisecondsPerDay);
  EXPECT_EQ(time_period1.end_timestamp() - time_period1.start_timestamp(),
            kMillisecondsPerDay);
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesKeptUntilSubmittedSuccessfully) {
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
  };
  // Make sure CPU stats get reported in time. If we don't run this, the second
  // call to |GetStatus()| will contain these stats, but the first call won't
  // and the EXPECT_EQ test below fails.
  base::RunLoop().RunUntilIdle();

  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  status_collector_->Simulate(test_states, 2);
  GetStatus();
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  em::DeviceStatusReportRequest first_status(device_status_);

  // The collector returns the same activity times again.
  GetStatus();
  int period_count = first_status.active_period_size();
  EXPECT_EQ(period_count, device_status_.active_period_size());
  for (int n = 0; n < period_count; ++n) {
    EXPECT_EQ(first_status.active_period(n).SerializeAsString(),
              device_status_.active_period(n).SerializeAsString());
  }

  // After indicating a successful submit, the submitted status gets cleared,
  // but what got collected meanwhile sticks around.
  status_collector_->Simulate(test_states, 1);
  status_collector_->OnSubmittedSuccessfully();
  GetStatus();
  EXPECT_EQ(ActivePeriodMilliseconds(), GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityNoUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_TRUE(device_status_.active_period(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithPublicSessionUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);
  const AccountId public_account_id(
      AccountId::FromUserEmail("public@localhost"));
  user_manager_->CreatePublicAccountUser(public_account_id);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_TRUE(device_status_.active_period(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithAffiliatedUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, true,
                                               user_manager::USER_TYPE_REGULAR);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(account_id0.GetUserEmail(),
            device_status_.active_period(0).user_email());
  device_status_.clear_active_period();  // Clear the result protobuf.

  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, false);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_TRUE(device_status_.active_period(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithNotAffiliatedUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, false,
                                               user_manager::USER_TYPE_REGULAR);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_TRUE(device_status_.active_period(0).user_email().empty());
  device_status_.clear_active_period();  // Clear the result protobuf.

  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, false);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_TRUE(device_status_.active_period(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  // Test that boot mode data is reported by default.
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueVerified);
  GetStatus();
  EXPECT_EQ("Verified", device_status_.boot_mode());

  // Test that boot mode data is not reported if the pref turned off.
  settings_helper_.SetBoolean(chromeos::kReportDeviceBootMode, false);

  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  settings_helper_.SetBoolean(chromeos::kReportDeviceBootMode, true);

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey, "(error)");
  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey, " ");
  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueVerified);
  GetStatus();
  EXPECT_EQ("Verified", device_status_.boot_mode());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueDev);
  GetStatus();
  EXPECT_EQ("Dev", device_status_.boot_mode());
}

TEST_F(DeviceStatusCollectorTest, WriteProtectSwitch) {
  // Test that write protect switch is reported by default.
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey,
      chromeos::system::kFirmwareWriteProtectBootValueOn);
  GetStatus();
  EXPECT_TRUE(device_status_.write_protect_switch());

  // Test that write protect switch is not reported if the hardware report pref
  // is off.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, false);

  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, true);

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey, "(error)");
  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey, " ");
  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey,
      chromeos::system::kFirmwareWriteProtectBootValueOn);
  GetStatus();
  EXPECT_TRUE(device_status_.write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey,
      chromeos::system::kFirmwareWriteProtectBootValueOff);
  GetStatus();
  EXPECT_FALSE(device_status_.write_protect_switch());
}

TEST_F(DeviceStatusCollectorTest, VersionInfo) {
  // Expect the version info to be reported by default.
  GetStatus();
  EXPECT_TRUE(device_status_.has_browser_version());
  EXPECT_TRUE(device_status_.has_channel());
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_TRUE(device_status_.has_firmware_version());
  EXPECT_TRUE(device_status_.has_tpm_version_info());

  // When the pref to collect this data is not enabled, expect that none of
  // the fields are present in the protobuf.
  settings_helper_.SetBoolean(chromeos::kReportDeviceVersionInfo, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_browser_version());
  EXPECT_FALSE(device_status_.has_channel());
  EXPECT_FALSE(device_status_.has_os_version());
  EXPECT_FALSE(device_status_.has_firmware_version());
  EXPECT_FALSE(device_status_.has_tpm_version_info());

  settings_helper_.SetBoolean(chromeos::kReportDeviceVersionInfo, true);
  GetStatus();
  EXPECT_TRUE(device_status_.has_browser_version());
  EXPECT_TRUE(device_status_.has_channel());
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_TRUE(device_status_.has_firmware_version());
  EXPECT_TRUE(device_status_.has_tpm_version_info());

  // Check that the browser version is not empty. OS version & firmware don't
  // have any reasonable values inside the unit test, so those aren't checked.
  EXPECT_NE("", device_status_.browser_version());
}

TEST_F(DeviceStatusCollectorTest, ReportUsers) {
  const AccountId public_account_id(
      AccountId::FromUserEmail("public@localhost"));
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  const AccountId account_id1(AccountId::FromUserEmail("user1@managed.com"));
  const AccountId account_id2(AccountId::FromUserEmail("user2@managed.com"));
  const AccountId account_id3(AccountId::FromUserEmail("user3@unmanaged.com"));
  const AccountId account_id4(AccountId::FromUserEmail("user4@managed.com"));
  const AccountId account_id5(AccountId::FromUserEmail("user5@managed.com"));

  user_manager_->CreatePublicAccountUser(public_account_id);
  user_manager_->AddUserWithAffiliationAndType(account_id0, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id1, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id2, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id3, false,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id4, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id5, true,
                                               user_manager::USER_TYPE_REGULAR);

  // Verify that users are reported by default.
  GetStatus();
  EXPECT_EQ(6, device_status_.user_size());

  // Verify that users are reported after enabling the setting.
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);
  GetStatus();
  EXPECT_EQ(6, device_status_.user_size());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.user(0).type());
  EXPECT_EQ(account_id0.GetUserEmail(), device_status_.user(0).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.user(1).type());
  EXPECT_EQ(account_id1.GetUserEmail(), device_status_.user(1).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.user(2).type());
  EXPECT_EQ(account_id2.GetUserEmail(), device_status_.user(2).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_UNMANAGED, device_status_.user(3).type());
  EXPECT_FALSE(device_status_.user(3).has_email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.user(4).type());
  EXPECT_EQ(account_id4.GetUserEmail(), device_status_.user(4).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.user(5).type());
  EXPECT_EQ(account_id5.GetUserEmail(), device_status_.user(5).email());

  // Verify that users are no longer reported if setting is disabled.
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.user_size());
}

TEST_F(DeviceStatusCollectorTest, TestVolumeInfo) {
  std::vector<std::string> expected_mount_points;
  std::vector<em::VolumeInfo> expected_volume_info;
  int size = 12345678;
  for (const auto& mount_info :
       DiskMountManager::GetInstance()->mount_points()) {
    expected_mount_points.push_back(mount_info.first);
  }
  expected_mount_points.push_back(kExternalMountPoint);

  for (const std::string& mount_point : expected_mount_points) {
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    // Just put unique numbers in for storage_total/free.
    info.set_storage_total(size++);
    info.set_storage_free(size++);
    expected_volume_info.push_back(info);
  }
  EXPECT_FALSE(expected_volume_info.empty());

  RestartStatusCollector(
      base::BindRepeating(&GetFakeVolumeInfo, expected_volume_info),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();

  GetStatus();
  EXPECT_EQ(expected_mount_points.size(),
            static_cast<size_t>(device_status_.volume_info_size()));

  // Walk the returned VolumeInfo to make sure it matches.
  for (const em::VolumeInfo& expected_info : expected_volume_info) {
    bool found = false;
    for (const em::VolumeInfo& info : device_status_.volume_info()) {
      if (info.volume_id() == expected_info.volume_id()) {
        EXPECT_EQ(expected_info.storage_total(), info.storage_total());
        EXPECT_EQ(expected_info.storage_free(), info.storage_free());
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "No matching VolumeInfo for "
                       << expected_info.volume_id();
  }

  // Now turn off hardware status reporting - should have no data.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.volume_info_size());
}

TEST_F(DeviceStatusCollectorTest, TestAvailableMemory) {
  // Refresh our samples. Sample more than kMaxHardwareSamples times to
  // make sure that the code correctly caps the number of cached samples.
  for (int i = 0; i < static_cast<int>(
                          DeviceStatusCollector::kMaxResourceUsageSamples + 1);
       ++i) {
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
  }
  GetStatus();
  EXPECT_EQ(static_cast<int>(DeviceStatusCollector::kMaxResourceUsageSamples),
            device_status_.system_ram_free().size());
  EXPECT_TRUE(device_status_.has_system_ram_total());
  // No good way to inject specific test values for available system RAM, so
  // just make sure it's > 0.
  EXPECT_GT(device_status_.system_ram_total(), 0);
}

TEST_F(DeviceStatusCollectorTest, TestCPUSamples) {
  // Mock 100% CPU usage.
  std::string full_cpu_usage("cpu  500 0 500 0 0 0 0");
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetFakeCPUStatistics, full_cpu_usage),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  GetStatus();
  ASSERT_EQ(1, device_status_.cpu_utilization_pct().size());
  EXPECT_EQ(100, device_status_.cpu_utilization_pct(0));

  // Now sample CPU usage again (active usage counters will not increase
  // so should show 0% cpu usage).
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();
  GetStatus();
  ASSERT_EQ(2, device_status_.cpu_utilization_pct().size());
  EXPECT_EQ(0, device_status_.cpu_utilization_pct(1));

  // Now store a bunch of 0% cpu usage and make sure we cap the max number of
  // samples.
  for (int i = 0;
       i < static_cast<int>(DeviceStatusCollector::kMaxResourceUsageSamples);
       ++i) {
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
  }
  GetStatus();

  // Should not be more than kMaxResourceUsageSamples, and they should all show
  // the CPU is idle.
  EXPECT_EQ(static_cast<int>(DeviceStatusCollector::kMaxResourceUsageSamples),
            device_status_.cpu_utilization_pct().size());
  for (const auto utilization : device_status_.cpu_utilization_pct())
    EXPECT_EQ(0, utilization);

  // Turning off hardware reporting should not report CPU utilization.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.cpu_utilization_pct().size());
}

TEST_F(DeviceStatusCollectorTest, TestCPUTemp) {
  std::vector<em::CPUTempInfo> expected_temp_info;
  int cpu_cnt = 12;
  for (int i = 0; i < cpu_cnt; ++i) {
    em::CPUTempInfo info;
    info.set_cpu_temp(i * 10 + 100);
    info.set_cpu_label(base::StringPrintf("Core %d", i));
    expected_temp_info.push_back(info);
  }

  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetFakeCPUTempInfo, expected_temp_info),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();

  GetStatus();
  EXPECT_EQ(expected_temp_info.size(),
            static_cast<size_t>(device_status_.cpu_temp_info_size()));

  // Walk the returned CPUTempInfo to make sure it matches.
  for (const em::CPUTempInfo& expected_info : expected_temp_info) {
    bool found = false;
    for (const em::CPUTempInfo& info : device_status_.cpu_temp_info()) {
      if (info.cpu_label() == expected_info.cpu_label()) {
        EXPECT_EQ(expected_info.cpu_temp(), info.cpu_temp());
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "No matching CPUTempInfo for "
                       << expected_info.cpu_label();
  }

  // Now turn off hardware status reporting - should have no data.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.cpu_temp_info_size());
}

TEST_F(DeviceStatusCollectorTest, KioskAndroidReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();
  EXPECT_EQ(kArcStatus, session_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo,
            session_status_.android_status().droid_guard_info());
  // Expect no User DM Token for kiosk sessions.
  EXPECT_FALSE(session_status_.has_user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, NoKioskAndroidReportingWhenDisabled) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  // Mock Kiosk app, so some session status is reported
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  // Note that this relies on the fact that kReportArcStatusEnabled is false by
  // default.
  EXPECT_FALSE(session_status_.has_android_status());
}

TEST_F(DeviceStatusCollectorTest, RegularUserAndroidReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  EXPECT_EQ(kArcStatus, session_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo,
            session_status_.android_status().droid_guard_info());
  // In tests, GetUserDMToken returns the e-mail for easy verification.
  EXPECT_EQ(account_id.GetUserEmail(), session_status_.user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, RegularUserCrostiniReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);
  testing_profile_->GetPrefs()->SetString(
      crostini::prefs::kCrostiniLastLaunchVersion, "1.33.7");
  testing_profile_->GetPrefs()->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart, 1535760000000);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  EXPECT_EQ(1535760000000, session_status_.crostini_status()
                               .last_launch_time_window_start_timestamp());
  EXPECT_EQ("1.33.7",
            session_status_.crostini_status().last_launch_vm_image_version());
  // In tests, GetUserDMToken returns the e-mail for easy verification.
  EXPECT_EQ(account_id.GetUserEmail(), session_status_.user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, RegularUserCrostiniReportingNoData) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);

  GetStatus();
  // Currently, only AndroidStatus and Crostini usage reporting is done for
  // regular users. If there is no reporting in both cases, no
  // UserSessionStatusRequest is filled at all. Note that this test case relies
  // on the fact that kReportArcStatusEnabled is false by default.
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, NoRegularUserReportingByDefault) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);

  GetStatus();
  // Currently, only AndroidStatus and Crostini usage reporting is done for
  // regular users. If both are disabled, no UserSessionStatusRequest is filled
  // at all. Note that this test case relies on the fact that
  // kReportArcStatusEnabled and kReportCrostiniUsageEnabled are false by
  // default.
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest,
       NoRegularUserAndroidReportingWhenNotAffiliated) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, false);
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();
  // Currently, only AndroidStatus reporting is done for regular users. If that
  // is disabled, no UserSessionStatusRequest is filled at all.
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, TpmStatusReporting) {
  // Create a fake TPM status info and populate it with some random values.
  const policy::TpmStatusInfo kFakeTpmStatus{
      true,  /* enabled */
      false, /* owned */
      true,  /* initialized */
      false, /* attestation_prepared */
      true,  /* attestation_enrolled */
      5,     /* dictionary_attack_counter */
      10,    /* dictionary_attack_threshold */
      false, /* dictionary_attack_lockout_in_effect */
      0,     /* dictionary_attack_lockout_seconds_remaining */
      true   /* boot_lockbox_finalized */
  };
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetFakeTpmStatus, kFakeTpmStatus));

  GetStatus();

  EXPECT_TRUE(device_status_.has_tpm_status_info());
  EXPECT_EQ(kFakeTpmStatus.enabled, device_status_.tpm_status_info().enabled());
  EXPECT_EQ(kFakeTpmStatus.owned, device_status_.tpm_status_info().owned());
  EXPECT_EQ(kFakeTpmStatus.initialized,
            device_status_.tpm_status_info().tpm_initialized());
  EXPECT_EQ(kFakeTpmStatus.attestation_prepared,
            device_status_.tpm_status_info().attestation_prepared());
  EXPECT_EQ(kFakeTpmStatus.attestation_enrolled,
            device_status_.tpm_status_info().attestation_enrolled());
  EXPECT_EQ(kFakeTpmStatus.dictionary_attack_counter,
            device_status_.tpm_status_info().dictionary_attack_counter());
  EXPECT_EQ(kFakeTpmStatus.dictionary_attack_threshold,
            device_status_.tpm_status_info().dictionary_attack_threshold());
  EXPECT_EQ(
      kFakeTpmStatus.dictionary_attack_lockout_in_effect,
      device_status_.tpm_status_info().dictionary_attack_lockout_in_effect());
  EXPECT_EQ(kFakeTpmStatus.dictionary_attack_lockout_seconds_remaining,
            device_status_.tpm_status_info()
                .dictionary_attack_lockout_seconds_remaining());
  EXPECT_EQ(kFakeTpmStatus.boot_lockbox_finalized,
            device_status_.tpm_status_info().boot_lockbox_finalized());
}

TEST_F(DeviceStatusCollectorTest, NoTimeZoneReporting) {
  // Time zone is not reported in enterprise reports.
  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);

  GetStatus();

  EXPECT_FALSE(session_status_.has_time_zone());
}

TEST_F(DeviceStatusCollectorTest, NoSessionStatusIfNoSession) {
  // Should not report session status if we don't have an active kiosk app or an
  // active user session.
  settings_helper_.SetBoolean(chromeos::kReportDeviceSessionStatus, true);
  GetStatus();
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, NoSessionStatusIfSessionReportingDisabled) {
  // Should not report session status if session status reporting is disabled.
  settings_helper_.SetBoolean(chromeos::kReportDeviceSessionStatus, false);
  // ReportDeviceSessionStatus only controls Kiosk reporting, ARC reporting
  // has to be disabled serarately.
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));
  // Set up a device-local account for single-app kiosk mode.
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           false);

  GetStatus();
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, ReportKioskSessionStatus) {
  settings_helper_.SetBoolean(chromeos::kReportDeviceSessionStatus, true);
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));

  // Set up a device-local account for single-app kiosk mode.
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  ASSERT_EQ(1, session_status_.installed_apps_size());
  EXPECT_EQ(kKioskAccountId, session_status_.device_local_account_id());
  const em::AppStatus app = session_status_.installed_apps(0);
  EXPECT_EQ(kKioskAppId, app.app_id());
  // Test code just sets the version to the app ID.
  EXPECT_EQ(kKioskAppId, app.extension_version());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
  // Expect no User DM Token for kiosk sessions.
  EXPECT_FALSE(session_status_.has_user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, ReportArcKioskSessionStatus) {
  settings_helper_.SetBoolean(chromeos::kReportDeviceSessionStatus, true);
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_arc_kiosk_device_local_account_));

  // Set up a device-local account for single-app ARC kiosk mode.
  MockRunningKioskApp(fake_arc_kiosk_device_local_account_,
                      true /* arc_kiosk */);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  ASSERT_EQ(1, session_status_.installed_apps_size());
  EXPECT_EQ(kArcKioskAccountId, session_status_.device_local_account_id());
  const em::AppStatus app = session_status_.installed_apps(0);
  EXPECT_EQ(kArcKioskPackageName, app.app_id());
  EXPECT_TRUE(app.extension_version().empty());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
  // Expect no User DM Token for kiosk sessions.
  EXPECT_FALSE(session_status_.has_user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, NoOsUpdateStatusByDefault) {
  MockPlatformVersion("1234.0.0");
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1234.0.0");

  GetStatus();
  EXPECT_FALSE(device_status_.has_os_update_status());
}

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatusUpToDate) {
  MockPlatformVersion("1234.0.0");
  settings_helper_.SetBoolean(chromeos::kReportOsUpdateStatus, true);

  const char* kRequiredPlatformVersions[] = {"1234", "1234.0", "1234.0.0"};

  for (size_t i = 0; i < arraysize(kRequiredPlatformVersions); ++i) {
    MockAutoLaunchKioskAppWithRequiredPlatformVersion(
        fake_kiosk_device_local_account_, kRequiredPlatformVersions[i]);

    GetStatus();
    ASSERT_TRUE(device_status_.has_os_update_status())
        << "Required platform version=" << kRequiredPlatformVersions[i];
    EXPECT_EQ(em::OsUpdateStatus::OS_UP_TO_DATE,
              device_status_.os_update_status().update_status())
        << "Required platform version=" << kRequiredPlatformVersions[i];
    EXPECT_EQ(kRequiredPlatformVersions[i],
              device_status_.os_update_status().new_required_platform_version())
        << "Required platform version=" << kRequiredPlatformVersions[i];
  }
}

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatus) {
  MockPlatformVersion("1234.0.0");
  settings_helper_.SetBoolean(chromeos::kReportOsUpdateStatus, true);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1235");

  chromeos::UpdateEngineClient::Status update_status;
  update_status.status = chromeos::UpdateEngineClient::UPDATE_STATUS_IDLE;

  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED,
            device_status_.os_update_status().update_status());

  const chromeos::UpdateEngineClient::UpdateStatusOperation kUpdateEngineOps[] =
      {
          chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING,
          chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING,
          chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING,
      };

  for (size_t i = 0; i < arraysize(kUpdateEngineOps); ++i) {
    update_status.status = kUpdateEngineOps[i];
    update_status.new_version = "1235.1.2";
    update_engine_client_->PushLastStatus(update_status);

    GetStatus();
    ASSERT_TRUE(device_status_.has_os_update_status());
    EXPECT_EQ(em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS,
              device_status_.os_update_status().update_status());
    EXPECT_EQ("1235.1.2",
              device_status_.os_update_status().new_platform_version());
    EXPECT_EQ(
        "1235",
        device_status_.os_update_status().new_required_platform_version());
  }

  update_status.status =
      chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  update_engine_client_->PushLastStatus(update_status);
  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT,
            device_status_.os_update_status().update_status());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppByDefault) {
  MockPlatformVersion("1234.0.0");
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1234.0.0");
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);

  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppWhenNotInKioskSession) {
  settings_helper_.SetBoolean(chromeos::kReportRunningKioskApp, true);
  MockPlatformVersion("1234.0.0");
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1234.0.0");

  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, ReportRunningKioskApp) {
  settings_helper_.SetBoolean(chromeos::kReportRunningKioskApp, true);
  MockPlatformVersion("1234.0.0");
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1235");
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));

  GetStatus();
  ASSERT_TRUE(device_status_.has_running_kiosk_app());
  const em::AppStatus app = device_status_.running_kiosk_app();
  EXPECT_EQ(kKioskAppId, app.app_id());
  EXPECT_EQ("1235", app.required_platform_version());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
}

TEST_F(DeviceStatusCollectorTest, ReportRunningArcKioskApp) {
  settings_helper_.SetBoolean(chromeos::kReportRunningKioskApp, true);
  MockAutoLaunchArcKioskApp(fake_arc_kiosk_device_local_account_);
  MockRunningKioskApp(fake_arc_kiosk_device_local_account_,
                      true /* arc_kiosk */);
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_arc_kiosk_device_local_account_));

  GetStatus();
  ASSERT_TRUE(device_status_.has_running_kiosk_app());
  const em::AppStatus app = device_status_.running_kiosk_app();
  EXPECT_EQ(kArcKioskPackageName, app.app_id());
  EXPECT_TRUE(app.extension_version().empty());
  EXPECT_TRUE(app.required_platform_version().empty());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
}

TEST_F(DeviceStatusCollectorTest, TestSoundVolume) {
  // Expect the sound volume to be reported by default (default sound volume
  // used in testing is 75).
  GetStatus();
  EXPECT_EQ(75, device_status_.sound_volume());

  // When the pref to collect this data is not enabled, expect that the field
  // isn't present in the protobuf.
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_sound_volume());

  // Try setting a custom volume value and check that it matches.
  const int kCustomVolume = 42;
  settings_helper_.SetBoolean(chromeos::kReportDeviceHardwareStatus, true);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(kCustomVolume);
  GetStatus();
  EXPECT_EQ(kCustomVolume, device_status_.sound_volume());
}

// Fake device state.
struct FakeDeviceData {
  const char* device_path;
  const char* type;
  const char* object_path;
  const char* mac_address;
  const char* meid;
  const char* imei;
  int expected_type;  // proto enum type value, -1 for not present.
};

static const FakeDeviceData kFakeDevices[] = {
  { "/device/ethernet", shill::kTypeEthernet, "ethernet",
    "112233445566", "", "",
    em::NetworkInterface::TYPE_ETHERNET },
  { "/device/cellular1", shill::kTypeCellular, "cellular1",
    "abcdefabcdef", "A10000009296F2", "",
    em::NetworkInterface::TYPE_CELLULAR },
  { "/device/cellular2", shill::kTypeCellular, "cellular2",
    "abcdefabcdef", "", "352099001761481",
    em::NetworkInterface::TYPE_CELLULAR },
  { "/device/wifi", shill::kTypeWifi, "wifi",
    "aabbccddeeff", "", "",
    em::NetworkInterface::TYPE_WIFI },
  { "/device/bluetooth", shill::kTypeBluetooth, "bluetooth",
    "", "", "",
    em::NetworkInterface::TYPE_BLUETOOTH },
  { "/device/vpn", shill::kTypeVPN, "vpn",
    "", "", "",
    -1 },
};

// Fake network state.
struct FakeNetworkState {
  const char* name;
  const char* device_path;
  const char* type;
  int signal_strength;
  int expected_signal_strength;
  const char* connection_status;
  int expected_state;
  const char* address;
  const char* gateway;
};

// List of fake networks - primarily used to make sure that signal strength
// and connection state are properly populated in status reports. Note that
// by convention shill will not report a signal strength of 0 for a visible
// network, so we use 1 below.
static const FakeNetworkState kFakeNetworks[] = {
  { "offline", "/device/wifi", shill::kTypeWifi, 35, -85,
    shill::kStateOffline, em::NetworkState::OFFLINE, "", "" },
  { "ethernet", "/device/ethernet", shill::kTypeEthernet, 0, 0,
    shill::kStateOnline, em::NetworkState::ONLINE,
    "192.168.0.1", "8.8.8.8" },
  { "wifi", "/device/wifi", shill::kTypeWifi, 23, -97, shill::kStatePortal,
    em::NetworkState::PORTAL, "", "" },
  { "idle", "/device/cellular1", shill::kTypeCellular, 0, 0, shill::kStateIdle,
    em::NetworkState::IDLE, "", "" },
  { "carrier", "/device/cellular1", shill::kTypeCellular, 0, 0,
    shill::kStateCarrier, em::NetworkState::CARRIER, "", "" },
  { "association", "/device/cellular1", shill::kTypeCellular, 0, 0,
    shill::kStateAssociation, em::NetworkState::ASSOCIATION, "", "" },
  { "config", "/device/cellular1", shill::kTypeCellular, 0, 0,
    shill::kStateConfiguration, em::NetworkState::CONFIGURATION, "", "" },
  // Set signal strength for this network to -20, but expected strength to 0
  // to test that we only report signal_strength for wifi connections.
  { "ready", "/device/cellular1", shill::kTypeCellular, -20, 0,
    shill::kStateReady, em::NetworkState::READY, "", "" },
  { "disconnect", "/device/wifi", shill::kTypeWifi, 1, -119,
    shill::kStateDisconnect, em::NetworkState::DISCONNECT, "", "" },
  { "failure", "/device/wifi", shill::kTypeWifi, 1, -119, shill::kStateFailure,
    em::NetworkState::FAILURE, "", "" },
  { "activation-failure", "/device/cellular1", shill::kTypeCellular, 0, 0,
    shill::kStateActivationFailure, em::NetworkState::ACTIVATION_FAILURE,
    "", "" },
  { "unknown", "", shill::kTypeWifi, 1, -119, "unknown",
    em::NetworkState::UNKNOWN, "", "" },
};

static const FakeNetworkState kUnconfiguredNetwork = {
  "unconfigured", "/device/unconfigured", shill::kTypeWifi, 35, -85,
  shill::kStateOffline, em::NetworkState::OFFLINE, "", ""
};

// Tests activity reporting day start correctness.
class DeviceStatusCollectorDayStartTest : public DeviceStatusCollectorTest {
 protected:
  DeviceStatusCollectorDayStartTest() = default;
  ~DeviceStatusCollectorDayStartTest() override = default;

  void SetUp() override {
    DeviceStatusCollectorTest::SetUp();
    settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  }

  // Restarts device status collector for activity reporting tests with given
  // |activity_day_start|.
  void RestartStatusCollectorWithDayStart(TimeDelta activity_day_start) {
    status_collector_ = std::make_unique<TestingDeviceStatusCollector>(
        &local_state_, &fake_statistics_provider_,
        base::BindRepeating(&GetEmptyVolumeInfo),
        base::BindRepeating(&GetEmptyCPUStatistics),
        base::BindRepeating(&GetEmptyCPUTempInfo),
        base::BindRepeating(&GetEmptyAndroidStatus),
        base::BindRepeating(&GetEmptyTpmStatus), activity_day_start,
        true /* is_enterprise_reporting */);
  }

  // Sets current test time to |time_since_midnight|.
  void SetCurrentTime(TimeDelta time_since_midnight) {
    status_collector_->SetBaselineTime(Time::Now().LocalMidnight() +
                                       time_since_midnight);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollectorDayStartTest);
};

TEST_F(DeviceStatusCollectorDayStartTest, ArbitraryActivityDayStart) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE,
                                 ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE};

  const TimeDelta kNoon = TimeDelta::FromHours(12);
  RestartStatusCollectorWithDayStart(kNoon);

  // Test a single active sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  device_status_.clear_active_period();  // Clear the result protobuf.

  // Test multiple consecutive active samples.
  status_collector_->Simulate(test_states, 4);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorDayStartTest, ActivityCrossingDayStart) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE};

  const TimeDelta kDayStart = TimeDelta::FromHours(6);
  RestartStatusCollectorWithDayStart(kDayStart);
  // Set time to 10 seconds after day start.
  SetCurrentTime(kDayStart + TimeDelta::FromSeconds(10));
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(2, device_status_.active_period_size());

  em::ActiveTimePeriod period0 = device_status_.active_period(0);
  em::ActiveTimePeriod period1 = device_status_.active_period(1);
  EXPECT_EQ(ActivePeriodMilliseconds() - 10000, period0.active_duration());
  EXPECT_EQ(10000, period1.active_duration());

  em::TimePeriod time_period0 = period0.time_period();
  em::TimePeriod time_period1 = period1.time_period();
  EXPECT_EQ(time_period0.end_timestamp(), time_period1.start_timestamp());
  // Ensure that the start and end times for the period are a day apart.
  EXPECT_EQ(time_period0.end_timestamp() - time_period0.start_timestamp(),
            kMillisecondsPerDay);
  EXPECT_EQ(time_period1.end_timestamp() - time_period1.start_timestamp(),
            kMillisecondsPerDay);
}

TEST_F(DeviceStatusCollectorDayStartTest, ActivityDayStartChangesToLater) {
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
  };

  const TimeDelta kDayStart; /* Midnight */
  RestartStatusCollectorWithDayStart(kDayStart);
  // Set clock to 1h after day start and report 2 activities.
  SetCurrentTime(kDayStart + kHour);
  status_collector_->Simulate(test_states, 2);

  // Move day starts to later hour.
  const TimeDelta kLaterDayStart = kDayStart + TimeDelta::FromHours(6);
  RestartStatusCollectorWithDayStart(kLaterDayStart);
  // Set clock before day start and report 1 activity.
  SetCurrentTime(kLaterDayStart - kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(2, device_status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());

  // Set clock after day start and report 1 activity.
  SetCurrentTime(kLaterDayStart + kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(3, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(2).active_duration());
}

TEST_F(DeviceStatusCollectorDayStartTest, ActivityDayStartChangesToEarlier) {
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
  };

  const TimeDelta kDayStart = TimeDelta::FromHours(6);
  RestartStatusCollectorWithDayStart(kDayStart);
  // Set clock after day start and report 2 activities.
  SetCurrentTime(kDayStart + kHour);
  status_collector_->Simulate(test_states, 2);

  // Move day starts to earlier hour.
  const TimeDelta kEarlierDayStart = kDayStart - TimeDelta::FromHours(3);
  RestartStatusCollectorWithDayStart(kEarlierDayStart);
  // Set clock before day start and report 1 activity.
  SetCurrentTime(kEarlierDayStart - kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(2, device_status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());

  // Set clock after day start and report 1 activity.
  SetCurrentTime(kEarlierDayStart + kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(3, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            device_status_.active_period(2).active_duration());
}

TEST_F(DeviceStatusCollectorDayStartTest,
       ActivityDayStartGetsBackToTheSameValue) {
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
  };

  const TimeDelta kDayStart = TimeDelta::FromHours(0);
  RestartStatusCollectorWithDayStart(kDayStart);
  // Set clock after day start report 2 activities.
  SetCurrentTime(kDayStart + kHour);
  status_collector_->Simulate(test_states, 2);

  // Move day starts to later hour.
  const TimeDelta kLaterDayStart = kDayStart + TimeDelta::FromHours(6);
  RestartStatusCollectorWithDayStart(kLaterDayStart);
  // Set clock after day start and report 1 activity.
  SetCurrentTime(kLaterDayStart + kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(2, device_status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());

  // Move day start back.
  RestartStatusCollectorWithDayStart(kDayStart);
  // Progress clock from the previous report.
  SetCurrentTime(kLaterDayStart + 2 * kHour);
  status_collector_->Simulate(test_states, 1);

  GetStatus();

  ASSERT_EQ(2, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            device_status_.active_period(0).active_duration());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            device_status_.active_period(1).active_duration());
}

class DeviceStatusCollectorNetworkInterfacesTest
    : public DeviceStatusCollectorTest {
 protected:
  void SetUp() override {
    RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                           base::BindRepeating(&GetEmptyCPUStatistics),
                           base::BindRepeating(&GetEmptyCPUTempInfo),
                           base::BindRepeating(&GetEmptyAndroidStatus),
                           base::BindRepeating(&GetEmptyTpmStatus));

    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    chromeos::ShillDeviceClient::TestInterface* device_client =
        chromeos::DBusThreadManager::Get()
            ->GetShillDeviceClient()
            ->GetTestInterface();
    chromeos::ShillServiceClient::TestInterface* service_client =
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();
    chromeos::ShillIPConfigClient::TestInterface* ip_config_client =
        chromeos::DBusThreadManager::Get()
            ->GetShillIPConfigClient()
            ->GetTestInterface();

    device_client->ClearDevices();
    service_client->ClearServices();

    for (const FakeDeviceData& dev : kFakeDevices) {
      device_client->AddDevice(dev.device_path, dev.type, dev.object_path);
      if (*dev.mac_address) {
        device_client->SetDeviceProperty(
            dev.device_path, shill::kAddressProperty,
            base::Value(dev.mac_address), /*notify_changed=*/true);
      }
      if (*dev.meid) {
        device_client->SetDeviceProperty(dev.device_path, shill::kMeidProperty,
                                         base::Value(dev.meid),
                                         /*notify_changed=*/true);
      }
      if (*dev.imei) {
        device_client->SetDeviceProperty(dev.device_path, shill::kImeiProperty,
                                         base::Value(dev.imei),
                                         /*notify_changed=*/true);
      }
    }

    chromeos::DBusThreadManager::Get()
        ->GetShillProfileClient()
        ->GetTestInterface()
        ->AddProfile(kShillFakeProfilePath, kShillFakeUserhash);

    // Now add services for every fake network.
    for (const FakeNetworkState& fake_network : kFakeNetworks) {
      // Shill forces non-visible networks to report a disconnected state.
      bool is_visible =
          fake_network.connection_status != shill::kStateDisconnect;
      service_client->AddService(fake_network.name /* service_path */,
                                 fake_network.name /* guid */,
                                 fake_network.name, fake_network.type,
                                 fake_network.connection_status, is_visible);
      service_client->SetServiceProperty(
          fake_network.name, shill::kSignalStrengthProperty,
          base::Value(fake_network.signal_strength));
      service_client->SetServiceProperty(fake_network.name,
                                         shill::kDeviceProperty,
                                         base::Value(fake_network.device_path));
      // Set the profile so this shows up as a configured network.
      service_client->SetServiceProperty(fake_network.name,
                                         shill::kProfileProperty,
                                         base::Value(kShillFakeProfilePath));
      if (strlen(fake_network.address) > 0) {
        // Set the IP config.
        base::DictionaryValue ip_config_properties;
        ip_config_properties.SetKey(shill::kAddressProperty,
                                    base::Value(fake_network.address));
        ip_config_properties.SetKey(shill::kGatewayProperty,
                                    base::Value(fake_network.gateway));
        const std::string kIPConfigPath = "test_ip_config";
        ip_config_client->AddIPConfig(kIPConfigPath, ip_config_properties);
        service_client->SetServiceProperty(fake_network.name,
                                           shill::kIPConfigProperty,
                                           base::Value(kIPConfigPath));
      }
    }

    // Now add an unconfigured network - it should not show up in the
    // reported list of networks because it doesn't have a profile specified.
    service_client->AddService(kUnconfiguredNetwork.name, /* service_path */
                               kUnconfiguredNetwork.name /* guid */,
                               kUnconfiguredNetwork.name /* name */,
                               kUnconfiguredNetwork.type /* type */,
                               kUnconfiguredNetwork.connection_status,
                               true /* visible */);
    service_client->SetServiceProperty(
        kUnconfiguredNetwork.name, shill::kSignalStrengthProperty,
        base::Value(kUnconfiguredNetwork.signal_strength));
    service_client->SetServiceProperty(
        kUnconfiguredNetwork.name, shill::kDeviceProperty,
        base::Value(kUnconfiguredNetwork.device_path));

    // Flush out pending state updates.
    base::RunLoop().RunUntilIdle();

    chromeos::NetworkStateHandler::NetworkStateList state_list;
    chromeos::NetworkStateHandler* network_state_handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    network_state_handler->GetNetworkListByType(
        chromeos::NetworkTypePattern::Default(),
        true,   // configured_only
        false,  // visible_only,
        0,      // no limit to number of results
        &state_list);
    ASSERT_EQ(arraysize(kFakeNetworks), state_list.size());
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
  }

  void VerifyNetworkReporting() {
    int count = 0;
    for (const FakeDeviceData& dev : kFakeDevices) {
      if (dev.expected_type == -1)
        continue;

      // Find the corresponding entry in reporting data.
      bool found_match = false;
      google::protobuf::RepeatedPtrField<em::NetworkInterface>::const_iterator
          iface;
      for (iface = device_status_.network_interface().begin();
           iface != device_status_.network_interface().end(); ++iface) {
        // Check whether type, field presence and field values match.
        if (dev.expected_type == iface->type() &&
            iface->has_mac_address() == !!*dev.mac_address &&
            iface->has_meid() == !!*dev.meid &&
            iface->has_imei() == !!*dev.imei &&
            iface->mac_address() == dev.mac_address &&
            iface->meid() == dev.meid && iface->imei() == dev.imei &&
            iface->device_path() == dev.device_path) {
          found_match = true;
          break;
        }
      }

      EXPECT_TRUE(found_match)
          << "No matching interface for fake device " << dev.device_path;
      count++;
    }

    EXPECT_EQ(count, device_status_.network_interface_size());

    // Now make sure network state list is correct.
    EXPECT_EQ(arraysize(kFakeNetworks),
              static_cast<size_t>(device_status_.network_state_size()));
    for (const FakeNetworkState& state : kFakeNetworks) {
      bool found_match = false;
      for (const em::NetworkState& proto_state :
           device_status_.network_state()) {
        // Make sure every item has a matching entry in the proto.
        bool should_have_signal_strength = state.expected_signal_strength != 0;
        if (proto_state.has_device_path() == (strlen(state.device_path) > 0) &&
            proto_state.has_signal_strength() == should_have_signal_strength &&
            proto_state.signal_strength() == state.expected_signal_strength &&
            proto_state.connection_state() == state.expected_state) {
          if (proto_state.has_ip_address())
            EXPECT_EQ(proto_state.ip_address(), state.address);
          else
            EXPECT_EQ(0U, strlen(state.address));
          if (proto_state.has_gateway())
            EXPECT_EQ(proto_state.gateway(), state.gateway);
          else
            EXPECT_EQ(0U, strlen(state.gateway));
          found_match = true;
          break;
        }
      }
      EXPECT_TRUE(found_match) << "No matching state for fake network "
                               << " (" << state.name << ")";
    }
  }
};

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, NoNetworkStateIfNotKiosk) {
  // If not in an active kiosk session, there should be network interfaces
  // reported, but no network state.
  GetStatus();
  EXPECT_LT(0, device_status_.network_interface_size());
  EXPECT_EQ(0, device_status_.network_state_size());
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, NetworkInterfaces) {
  // Mock that we are in kiosk mode so we report network state.
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));

  // Interfaces should be reported by default.
  GetStatus();
  EXPECT_LT(0, device_status_.network_interface_size());
  EXPECT_LT(0, device_status_.network_state_size());

  // No interfaces should be reported if the policy is off.
  settings_helper_.SetBoolean(chromeos::kReportDeviceNetworkInterfaces, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.network_interface_size());
  EXPECT_EQ(0, device_status_.network_state_size());

  // Switch the policy on and verify the interface list is present.
  settings_helper_.SetBoolean(chromeos::kReportDeviceNetworkInterfaces, true);
  GetStatus();

  VerifyNetworkReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, ReportIfPublicSession) {
  // Report netowork state for public accounts.
  user_manager_->CreatePublicAccountUser(
      AccountId::FromUserEmail(kPublicAccountId));
  EXPECT_CALL(*user_manager_, IsLoggedInAsPublicAccount())
      .WillRepeatedly(Return(true));

  settings_helper_.SetBoolean(chromeos::kReportDeviceNetworkInterfaces, true);
  GetStatus();
  VerifyNetworkReporting();
}

// Tests collecting device status for registered consumer device.
class ConsumerDeviceStatusCollectorTimeLimitDisabledTest
    : public DeviceStatusCollectorTest {
 public:
  ConsumerDeviceStatusCollectorTimeLimitDisabledTest() {
    user_account_id_ = AccountId::FromUserEmail("user0@gmail.com");
    MockChildUser(user_account_id_);
    scoped_feature_list_.InitAndDisableFeature(features::kUsageTimeLimitPolicy);
  }

  ~ConsumerDeviceStatusCollectorTimeLimitDisabledTest() override = default;

 protected:
  void RestartStatusCollector(
      const policy::DeviceStatusCollector::VolumeInfoFetcher& volume_info,
      const policy::DeviceStatusCollector::CPUStatisticsFetcher& cpu_stats,
      const policy::DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher,
      const policy::DeviceStatusCollector::AndroidStatusFetcher&
          android_status_fetcher,
      const policy::DeviceStatusCollector::TpmStatusFetcher& tpm_status_fetcher)
      override {
    status_collector_ = std::make_unique<TestingDeviceStatusCollector>(
        &profile_pref_service_, &fake_statistics_provider_, volume_info,
        cpu_stats, cpu_temp_fetcher, android_status_fetcher, tpm_status_fetcher,
        kMidnight, false /* is_enterprise_reporting */);
  }

  AccountId user_account_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void expectChildScreenTimeMilliseconds(int64_t duration,
                                       TestingPrefServiceSimple* pref_service) {
  pref_service->CommitPendingWrite(
      base::OnceClosure(),
      base::BindOnce(
          [](int64_t duration, TestingPrefServiceSimple* pref_service) {
            EXPECT_EQ(duration, pref_service->GetInteger(
                                    prefs::kChildScreenTimeMilliseconds));
          },
          duration, pref_service));
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest, ReportingBootMode) {
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueVerified);

  GetStatus();

  EXPECT_TRUE(device_status_.has_boot_mode());
  EXPECT_EQ("Verified", device_status_.boot_mode());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       NotReportingWriteProtectSwitch) {
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kFirmwareWriteProtectBootKey,
      chromeos::system::kFirmwareWriteProtectBootValueOn);

  GetStatus();

  EXPECT_FALSE(device_status_.has_write_protect_switch());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest, ReportingArcStatus) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus));

  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           true);

  GetStatus();

  EXPECT_EQ(kArcStatus, session_status_.android_status().status_payload());
  EXPECT_EQ(kDroidGuardInfo,
            session_status_.android_status().droid_guard_info());
  // In tests, GetUserDMToken returns the e-mail for easy verification.
  EXPECT_EQ(user_account_id_.GetUserEmail(), session_status_.user_dm_token());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       ReportingPartialVersionInfo) {
  GetStatus();

  // Should only report OS version.
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_FALSE(device_status_.has_browser_version());
  EXPECT_FALSE(device_status_.has_channel());
  EXPECT_FALSE(device_status_.has_firmware_version());
  EXPECT_FALSE(device_status_.has_tpm_version_info());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       NotReportingVolumeInfo) {
  std::vector<std::string> expected_mount_points;
  std::vector<em::VolumeInfo> expected_volume_info;
  for (const auto& mount_info :
       DiskMountManager::GetInstance()->mount_points()) {
    expected_mount_points.push_back(mount_info.first);
  }
  expected_mount_points.push_back(kExternalMountPoint);

  for (const std::string& mount_point : expected_mount_points) {
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    info.set_storage_total(12345678);
    info.set_storage_free(1234567);
    expected_volume_info.push_back(info);
  }
  EXPECT_FALSE(expected_volume_info.empty());

  RestartStatusCollector(
      base::BindRepeating(&GetFakeVolumeInfo, expected_volume_info),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus));
  content::RunAllTasksUntilIdle();

  GetStatus();

  EXPECT_EQ(0, device_status_.volume_info_size());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest, NotReportingUsers) {
  const AccountId account_id0(AccountId::FromUserEmail("user0@gmail.com"));
  const AccountId account_id1(AccountId::FromUserEmail("user1@gmail.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, true,
                                               user_manager::USER_TYPE_REGULAR);
  user_manager_->AddUserWithAffiliationAndType(account_id1, true,
                                               user_manager::USER_TYPE_CHILD);

  GetStatus();

  EXPECT_EQ(0, device_status_.user_size());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       NotReportingOSUpdateStatus) {
  MockPlatformVersion("1234.0.0");
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1234.0.0");

  GetStatus();

  EXPECT_FALSE(device_status_.has_os_update_status());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       NotReportingDeviceHardwareStatus) {
  const std::string full_cpu_usage("cpu  500 0 500 0");

  std::vector<em::CPUTempInfo> expected_temp_info;
  for (int i = 0; i < 5; ++i) {
    em::CPUTempInfo info;
    info.set_cpu_temp(100);
    info.set_cpu_label("Core");
    expected_temp_info.push_back(info);
  }

  status_collector_->RefreshSampleResourceUsage();

  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetFakeCPUStatistics, full_cpu_usage),
      base::BindRepeating(&GetFakeCPUTempInfo, expected_temp_info),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus));
  content::RunAllTasksUntilIdle();

  GetStatus();

  EXPECT_FALSE(device_status_.has_sound_volume());
  EXPECT_EQ(0, device_status_.cpu_utilization_pct().size());
  EXPECT_EQ(0, device_status_.cpu_temp_info_size());
  EXPECT_EQ(0, device_status_.system_ram_free().size());
  EXPECT_FALSE(device_status_.has_system_ram_total());
  EXPECT_FALSE(device_status_.has_tpm_status_info());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest, TimeZoneReporting) {
  const std::string timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());

  GetStatus();

  EXPECT_TRUE(session_status_.has_time_zone());
  EXPECT_EQ(timezone, session_status_.time_zone());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitDisabledTest,
       ActivityTimesFeatureDisable) {
  settings_helper_.SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  settings_helper_.SetBoolean(chromeos::kReportDeviceUsers, true);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  status_collector_->Simulate(test_states, 3);

  GetStatus();
  EXPECT_EQ(0, device_status_.active_period_size());
}

// Tests collecting device status for registered consumer device when time
// limit feature is enabled.
class ConsumerDeviceStatusCollectorTimeLimitEnabledTest
    : public ConsumerDeviceStatusCollectorTimeLimitDisabledTest {
 public:
  ConsumerDeviceStatusCollectorTimeLimitEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kUsageTimeLimitPolicy);
  }
  ~ConsumerDeviceStatusCollectorTimeLimitEnabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fails on all chromeos builders https://crbug.com/891573
TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       DISABLED_ReportingActivityTimesSessionTransistions) {
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

  ASSERT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  expectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
  EXPECT_EQ(user_account_id_.GetUserEmail(),
            device_status_.active_period(0).user_email());
}

// Fails on all chromeos builders https://crbug.com/891573
TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       DISABLED_ReportingActivityTimesSleepTransistions) {
  DeviceStateTransitions test_states[] = {
      DeviceStateTransitions::kEnterSessionActive,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kEnterSleep,
      DeviceStateTransitions::kPeriodicCheckTriggered,  // Check while inactive
      DeviceStateTransitions::kLeaveSleep,
      DeviceStateTransitions::kPeriodicCheckTriggered,
      DeviceStateTransitions::kLeaveSessionActive};
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();

  ASSERT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  expectChildScreenTimeMilliseconds(4 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
  EXPECT_EQ(user_account_id_.GetUserEmail(),
            device_status_.active_period(0).user_email());
}

// Fails on all chromeos builders https://crbug.com/891573
TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       DISABLED_ReportingActivityTimesIdleTransitions) {
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

  ASSERT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  expectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
  EXPECT_EQ(user_account_id_.GetUserEmail(),
            device_status_.active_period(0).user_email());
}

// Fails on all chromeos builders https://crbug.com/891573
TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       DISABLED_ActivityKeptInPref) {
  EXPECT_TRUE(
      profile_pref_service_.GetDictionary(prefs::kUserActivityTimes)->empty());

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
  RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                         base::BindRepeating(&GetEmptyCPUStatistics),
                         base::BindRepeating(&GetEmptyCPUTempInfo),
                         base::BindRepeating(&GetEmptyAndroidStatus),
                         base::BindRepeating(&GetEmptyTpmStatus));
  SimulateStateChanges(test_states,
                       sizeof(test_states) / sizeof(DeviceStateTransitions));

  GetStatus();
  EXPECT_EQ(12 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  expectChildScreenTimeMilliseconds(12 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
}

// Fails on all chromeos builders https://crbug.com/891573
TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       DISABLED_ActivityNotWrittenToLocalState) {
  EXPECT_TRUE(local_state_.GetDictionary(prefs::kDeviceActivityTimes)->empty());

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
  EXPECT_EQ(1, device_status_.active_period_size());
  EXPECT_EQ(5 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  expectChildScreenTimeMilliseconds(5 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
  // Nothing should be written to local state, because it is only used for
  // enterprise reporting.
  EXPECT_TRUE(local_state_.GetDictionary(prefs::kDeviceActivityTimes)->empty());
}

TEST_F(ConsumerDeviceStatusCollectorTimeLimitEnabledTest,
       ActivityCrossingMidnight) {
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
  ASSERT_EQ(2, device_status_.active_period_size());

  em::ActiveTimePeriod period0 = device_status_.active_period(0);
  em::ActiveTimePeriod period1 = device_status_.active_period(1);
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
  expectChildScreenTimeMilliseconds(0.5 * ActivePeriodMilliseconds(),
                                    &profile_pref_service_);
}

}  // namespace policy
