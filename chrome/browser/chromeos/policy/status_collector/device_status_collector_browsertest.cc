// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
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

// Constants for Crostini reporting test cases:
const char kCrostiniUserEmail[] = "user0@managed.com";
const char kTerminaVmComponentVersion[] = "1.33.7";
const char kTerminaVmKernelVersion[] =
    "4.19.56-05556-gca219a5b1086 #3 SMP PREEMPT Mon Jul 1 14:36:38 CEST 2019";
const char kActualLastLaunchTimeFormatted[] = "Sat, 1 Sep 2018 11:50:50 GMT";
const char kLastLaunchTimeWindowStartFormatted[] =
    "Sat, 1 Sep 2018 00:00:00 GMT";
const long kLastLaunchTimeWindowStartInJavaTime = 1535760000000;
const char kDefaultPlatformVersion[] = "1234.0.0";

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
      const policy::DeviceStatusCollector::EMMCLifetimeFetcher&
          emmc_lifetime_fetcher,
      const policy::DeviceStatusCollector::StatefulPartitionInfoFetcher&
          stateful_partition_info_fetcher,
      const policy::DeviceStatusCollector::CrosHealthdDataFetcher&
          cros_healthd_data_fetcher)
      : policy::DeviceStatusCollector(pref_service,
                                      provider,
                                      volume_info_fetcher,
                                      cpu_fetcher,
                                      cpu_temp_fetcher,
                                      android_status_fetcher,
                                      tpm_status_fetcher,
                                      emmc_lifetime_fetcher,
                                      stateful_partition_info_fetcher,
                                      cros_healthd_data_fetcher) {
    // Set the baseline time to a fixed value (1 hour after day start) to
    // prevent test flakiness due to a single activity period spanning two days.
    SetBaselineTime(Time::Now().LocalMidnight() + kHour);
  }

  void Simulate(ui::IdleState* states, int len) {
    for (int i = 0; i < len; i++)
      ProcessIdleState(states[i]);
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

  std::unique_ptr<policy::DeviceLocalAccount>
  GetAutoLaunchedKioskSessionInfo() override {
    if (kiosk_account_)
      return std::make_unique<policy::DeviceLocalAccount>(*kiosk_account_);
    return std::unique_ptr<policy::DeviceLocalAccount>();
  }

  std::string GetAppVersion(const std::string& app_id) override {
    // Just return the app_id as the version - this makes it easy for tests
    // to confirm that the correct app's version was requested.
    return app_id;
  }

  std::string GetDMTokenForProfile(Profile* profile) const override {
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
  for (int i = 0; i < status.active_periods_size(); i++) {
    active_milliseconds += status.active_periods(i).active_duration();
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

em::DiskLifetimeEstimation GetEmptyEMMCLifetimeEstimation() {
  return em::DiskLifetimeEstimation();
}

em::DiskLifetimeEstimation GetFakeEMMCLifetiemEstimation(
    const em::DiskLifetimeEstimation& value) {
  return value;
}

em::StatefulPartitionInfo GetEmptyStatefulPartitionInfo() {
  return em::StatefulPartitionInfo();
}

em::StatefulPartitionInfo GetFakeStatefulPartitionInfo(
    const em::StatefulPartitionInfo& value) {
  return value;
}

void GetEmptyCrosHealthdData(
    policy::DeviceStatusCollector::CrosHealthdDataReceiver receiver) {
  chromeos::cros_healthd::mojom::TelemetryInfoPtr empty_info;
  base::circular_deque<std::unique_ptr<policy::SampledData>> empty_samples;
  std::move(receiver).Run(std::move(empty_info), empty_samples);
}

void GetFakeCrosHealthdData(
    const chromeos::cros_healthd::mojom::BatteryInfo& battery_info,
    const chromeos::cros_healthd::mojom::CachedVpdInfo& cached_vpd_info,
    const chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfo&
        storage_info,
    const em::CPUTempInfo& cpu_sample,
    const em::BatterySample& battery_sample,
    policy::DeviceStatusCollector::CrosHealthdDataReceiver receiver) {
  std::vector<chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>
      storage_vector;
  storage_vector.push_back(storage_info.Clone());
  base::Optional<std::vector<
      chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>>
      block_device_info(std::move(storage_vector));
  chromeos::cros_healthd::mojom::TelemetryInfo fake_info(
      battery_info.Clone(), std::move(block_device_info),
      cached_vpd_info.Clone());

  auto sample = std::make_unique<policy::SampledData>();
  sample->cpu_samples[cpu_sample.cpu_label()] = cpu_sample;
  sample->battery_samples[battery_info.model_name] = battery_sample;
  base::circular_deque<std::unique_ptr<policy::SampledData>> samples;
  samples.push_back(std::move(sample));

  std::move(receiver).Run(fake_info.Clone(), samples);
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

    // Set up a fake local state for KioskAppManager and KioskCryptohomeRemover.
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    chromeos::KioskAppManager::RegisterPrefs(local_state_.registry());
    chromeos::KioskCryptohomeRemover::RegisterPrefs(local_state_.registry());

    // Use FakeUpdateEngineClient.
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        base::WrapUnique<chromeos::UpdateEngineClient>(update_engine_client_));

    chromeos::CrasAudioHandler::InitializeForTesting();
    chromeos::CryptohomeClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();
  }

  ~DeviceStatusCollectorTest() override {
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::CryptohomeClient::Shutdown();
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
                           base::BindRepeating(&GetEmptyTpmStatus),
                           base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
                           base::BindRepeating(&GetEmptyStatefulPartitionInfo),
                           base::BindRepeating(&GetEmptyCrosHealthdData));

    // Disable network interface reporting since it requires additional setup.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kReportDeviceNetworkInterfaces, false);
  }

  void TearDown() override { status_collector_.reset(); }

 protected:
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
      const policy::DeviceStatusCollector::TpmStatusFetcher& tpm_status_fetcher,
      const policy::DeviceStatusCollector::EMMCLifetimeFetcher&
          emmc_lifetime_fetcher,
      const policy::DeviceStatusCollector::StatefulPartitionInfoFetcher&
          stateful_partition_info_fetcher,
      const policy::DeviceStatusCollector::CrosHealthdDataFetcher&
          cros_healthd_data_fetcher) {
    std::vector<em::VolumeInfo> expected_volume_info;
    status_collector_ = std::make_unique<TestingDeviceStatusCollector>(
        &local_state_, &fake_statistics_provider_, volume_info, cpu_stats,
        cpu_temp_fetcher, android_status_fetcher, tpm_status_fetcher,
        emmc_lifetime_fetcher, stateful_partition_info_fetcher,
        cros_healthd_data_fetcher);
  }

  void GetStatus() {
    device_status_.Clear();
    session_status_.Clear();
    got_session_status_ = false;
    run_loop_.reset(new base::RunLoop());
    status_collector_->GetStatusAsync(base::BindRepeating(
        &DeviceStatusCollectorTest::OnStatusReceived, base::Unretained(this)));
    run_loop_->Run();
    run_loop_.reset();
  }

  void OnStatusReceived(StatusCollectorParams callback_params) {
    if (callback_params.device_status)
      device_status_ = *callback_params.device_status;
    got_session_status_ = callback_params.session_status != nullptr;
    if (got_session_status_)
      session_status_ = *callback_params.session_status;
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

    SetDeviceLocalAccounts(&owner_settings_service_, accounts);
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
    SetDeviceLocalAccounts(&owner_settings_service_, accounts);

    owner_settings_service_.SetString(
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
    SetDeviceLocalAccounts(&owner_settings_service_, accounts);

    owner_settings_service_.SetString(
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
  content::BrowserTaskEnvironment task_environment_;

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DiskMountManager::MountPointMap mount_point_map_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::FakeOwnerSettingsService owner_settings_service_{
      scoped_testing_cros_settings_.device_settings(), nullptr};
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;

  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollectorTest);
};

TEST_F(DeviceStatusCollectorTest, AllIdle) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE,
    ui::IDLE_STATE_IDLE
  };
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);

  // Test reporting with no data.
  GetStatus();
  EXPECT_EQ(0, device_status_.active_periods_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));

  // Test reporting with a single idle sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(0, device_status_.active_periods_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));

  // Test reporting with multiple consecutive idle samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(0, device_status_.active_periods_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, AllActive) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE
  };
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);

  // Test a single active sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  device_status_.clear_active_periods();  // Clear the result protobuf.

  // Test multiple consecutive active samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);

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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                         base::BindRepeating(&GetEmptyCPUStatistics),
                         base::BindRepeating(&GetEmptyCPUTempInfo),
                         base::BindRepeating(&GetEmptyAndroidStatus),
                         base::BindRepeating(&GetEmptyTpmStatus),
                         base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
                         base::BindRepeating(&GetEmptyStatefulPartitionInfo),
                         base::BindRepeating(&GetEmptyCrosHealthdData));
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
  EXPECT_EQ(1, device_status_.active_periods_size());
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

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
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
  EXPECT_EQ(kMaxDays - 1, device_status_.active_periods_size());

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
  device_status_.clear_active_periods();
  GetStatus();
  EXPECT_LT(device_status_.active_periods_size(), kMaxDays);
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
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesOff) {
  // Device activity times should not be reported if explicitly disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, false);
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE,
    ui::IDLE_STATE_ACTIVE
  };
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(0, device_status_.active_periods_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityCrossingMidnight) {
  ui::IdleState test_states[] = {
    ui::IDLE_STATE_ACTIVE
  };
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);

  // Set the baseline time to 10 seconds after midnight.
  status_collector_->SetBaselineTime(
      Time::Now().LocalMidnight() + TimeDelta::FromSeconds(10));

  status_collector_->Simulate(test_states, 1);
  GetStatus();
  ASSERT_EQ(2, device_status_.active_periods_size());

  em::ActiveTimePeriod period0 = device_status_.active_periods(0);
  em::ActiveTimePeriod period1 = device_status_.active_periods(1);
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

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);

  status_collector_->Simulate(test_states, 2);
  GetStatus();
  EXPECT_EQ(2 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
  em::DeviceStatusReportRequest first_status(device_status_);

  // The collector returns the same activity times again.
  GetStatus();
  int period_count = first_status.active_periods_size();
  EXPECT_EQ(period_count, device_status_.active_periods_size());
  for (int n = 0; n < period_count; ++n) {
    EXPECT_EQ(first_status.active_periods(n).SerializeAsString(),
              device_status_.active_periods(n).SerializeAsString());
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, true);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithPublicSessionUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, true);
  const AccountId public_account_id(
      AccountId::FromUserEmail("public@localhost"));
  user_manager_->CreatePublicAccountUser(public_account_id);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithAffiliatedUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, true,
                                               user_manager::USER_TYPE_REGULAR);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(account_id0.GetUserEmail(),
            device_status_.active_periods(0).user_email());
  device_status_.clear_active_periods();  // Clear the result protobuf.

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, false);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithNotAffiliatedUser) {
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  user_manager_->AddUserWithAffiliationAndType(account_id0, false,
                                               user_manager::USER_TYPE_REGULAR);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  device_status_.clear_active_periods();  // Clear the result protobuf.

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, false);

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  // Test that boot mode data is reported by default.
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kDevSwitchBootKey,
      chromeos::system::kDevSwitchBootValueVerified);
  GetStatus();
  EXPECT_EQ("Verified", device_status_.boot_mode());

  // Test that boot mode data is not reported if the pref turned off.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceBootMode, false);

  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceBootMode, true);

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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);

  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, true);

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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceVersionInfo, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_browser_version());
  EXPECT_FALSE(device_status_.has_channel());
  EXPECT_FALSE(device_status_.has_os_version());
  EXPECT_FALSE(device_status_.has_firmware_version());
  EXPECT_FALSE(device_status_.has_tpm_version_info());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceVersionInfo, true);
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
  EXPECT_EQ(6, device_status_.users_size());

  // Verify that users are reported after enabling the setting.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, true);
  GetStatus();
  EXPECT_EQ(6, device_status_.users_size());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.users(0).type());
  EXPECT_EQ(account_id0.GetUserEmail(), device_status_.users(0).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.users(1).type());
  EXPECT_EQ(account_id1.GetUserEmail(), device_status_.users(1).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.users(2).type());
  EXPECT_EQ(account_id2.GetUserEmail(), device_status_.users(2).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_UNMANAGED,
            device_status_.users(3).type());
  EXPECT_FALSE(device_status_.users(3).has_email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.users(4).type());
  EXPECT_EQ(account_id4.GetUserEmail(), device_status_.users(4).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, device_status_.users(5).type());
  EXPECT_EQ(account_id5.GetUserEmail(), device_status_.users(5).email());

  // Verify that users are no longer reported if setting is disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceUsers, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.users_size());
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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();

  GetStatus();
  EXPECT_EQ(expected_mount_points.size(),
            static_cast<size_t>(device_status_.volume_infos_size()));

  // Walk the returned VolumeInfo to make sure it matches.
  for (const em::VolumeInfo& expected_info : expected_volume_info) {
    bool found = false;
    for (const em::VolumeInfo& info : device_status_.volume_infos()) {
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.volume_infos_size());
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
            device_status_.system_ram_free_samples().size());
  EXPECT_TRUE(device_status_.has_system_ram_total());
  // No good way to inject specific test values for available system RAM, so
  // just make sure it's > 0.
  EXPECT_GT(device_status_.system_ram_total(), 0);
}

TEST_F(DeviceStatusCollectorTest, TestSystemFreeRamInfo) {
  const int sample_count =
      static_cast<const int>(DeviceStatusCollector::kMaxResourceUsageSamples);
  std::vector<int64_t> timestamp_lowerbounds;
  std::vector<int64_t> timestamp_upperbounds;

  // Refresh our samples. Sample more than kMaxHardwareSamples times to
  // make sure that the code correctly caps the number of cached samples.
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < sample_count; ++i) {
    timestamp_lowerbounds.push_back(base::Time::Now().ToJavaTime());
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
    timestamp_upperbounds.push_back(base::Time::Now().ToJavaTime());
  }
  GetStatus();

  EXPECT_TRUE(device_status_.has_system_ram_total());
  EXPECT_GT(device_status_.system_ram_total(), 0);

  EXPECT_EQ(sample_count, device_status_.system_ram_free_infos().size());
  for (int i = 0; i < sample_count; ++i) {
    // Make sure 0 < free RAM < total Ram.
    EXPECT_GT(device_status_.system_ram_free_infos(i).size_in_bytes(), 0);
    EXPECT_LT(device_status_.system_ram_free_infos(i).size_in_bytes(),
              device_status_.system_ram_total());

    // Make sure timestamp is in a valid range though we cannot inject specific
    // test values.
    EXPECT_GE(device_status_.system_ram_free_infos(i).timestamp(),
              timestamp_lowerbounds[i]);
    EXPECT_LE(device_status_.system_ram_free_infos(i).timestamp(),
              timestamp_upperbounds[i]);
  }
}

TEST_F(DeviceStatusCollectorTest, TestCPUSamples) {
  // Mock 100% CPU usage.
  std::string full_cpu_usage("cpu  500 0 500 0 0 0 0");
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetFakeCPUStatistics, full_cpu_usage),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  GetStatus();
  ASSERT_EQ(1, device_status_.cpu_utilization_pct_samples().size());
  EXPECT_EQ(100, device_status_.cpu_utilization_pct_samples(0));

  // Now sample CPU usage again (active usage counters will not increase
  // so should show 0% cpu usage).
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();
  GetStatus();
  ASSERT_EQ(2, device_status_.cpu_utilization_pct_samples().size());
  EXPECT_EQ(0, device_status_.cpu_utilization_pct_samples(1));

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
            device_status_.cpu_utilization_pct_samples().size());
  for (const auto utilization : device_status_.cpu_utilization_pct_samples())
    EXPECT_EQ(0, utilization);

  // Turning off hardware reporting should not report CPU utilization.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.cpu_utilization_pct_samples().size());
}

TEST_F(DeviceStatusCollectorTest, TestCPUInfos) {
  // Mock 100% CPU usage.
  std::string full_cpu_usage("cpu  500 0 500 0 0 0 0");
  int64_t timestamp_lowerbound = base::Time::Now().ToJavaTime();
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetFakeCPUStatistics, full_cpu_usage),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  int64_t timestamp_upperbound = base::Time::Now().ToJavaTime();
  GetStatus();
  ASSERT_EQ(1, device_status_.cpu_utilization_infos().size());
  EXPECT_EQ(100, device_status_.cpu_utilization_infos(0).cpu_utilization_pct());
  EXPECT_GE(device_status_.cpu_utilization_infos(0).timestamp(),
            timestamp_lowerbound);
  EXPECT_LE(device_status_.cpu_utilization_infos(0).timestamp(),
            timestamp_upperbound);

  // Now sample CPU usage again (active usage counters will not increase
  // so should show 0% cpu usage).
  timestamp_lowerbound = base::Time::Now().ToJavaTime();
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();
  timestamp_upperbound = base::Time::Now().ToJavaTime();
  GetStatus();
  ASSERT_EQ(2, device_status_.cpu_utilization_infos().size());
  EXPECT_EQ(0, device_status_.cpu_utilization_infos(1).cpu_utilization_pct());
  EXPECT_GE(device_status_.cpu_utilization_infos(1).timestamp(),
            timestamp_lowerbound);
  EXPECT_LE(device_status_.cpu_utilization_infos(1).timestamp(),
            timestamp_upperbound);

  // Now store a bunch of 0% cpu usage and make sure we cap the max number of
  // samples.
  const int sample_count =
      static_cast<const int>(DeviceStatusCollector::kMaxResourceUsageSamples);
  std::vector<int64_t> timestamp_lowerbounds;
  std::vector<int64_t> timestamp_upperbounds;

  for (int i = 0; i < sample_count; ++i) {
    timestamp_lowerbounds.push_back(base::Time::Now().ToJavaTime());
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
    timestamp_upperbounds.push_back(base::Time::Now().ToJavaTime());
  }
  GetStatus();

  // Should not be more than kMaxResourceUsageSamples, and they should all show
  // the CPU is idle.
  EXPECT_EQ(sample_count, device_status_.cpu_utilization_infos().size());
  for (int i = 0; i < sample_count; ++i) {
    EXPECT_EQ(device_status_.cpu_utilization_infos(i).cpu_utilization_pct(), 0);

    // Make sure timestamp is in a valid range though we cannot inject specific
    // test values.
    EXPECT_GE(device_status_.cpu_utilization_infos(i).timestamp(),
              timestamp_lowerbounds[i]);
    EXPECT_LE(device_status_.cpu_utilization_infos(i).timestamp(),
              timestamp_upperbounds[i]);
  }

  // Turning off hardware reporting should not report CPU utilization.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.cpu_utilization_infos().size());
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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();

  GetStatus();
  EXPECT_EQ(expected_temp_info.size(),
            static_cast<size_t>(device_status_.cpu_temp_infos_size()));

  // Walk the returned CPUTempInfo to make sure it matches.
  for (const em::CPUTempInfo& expected_info : expected_temp_info) {
    bool found = false;
    for (const em::CPUTempInfo& info : device_status_.cpu_temp_infos()) {
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.cpu_temp_infos_size());
}

TEST_F(DeviceStatusCollectorTest, TestDiskLifetimeEstimation) {
  em::DiskLifetimeEstimation est;
  est.set_slc(10);
  est.set_mlc(15);
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetFakeEMMCLifetiemEstimation, est),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceStorageStatus, true);
  GetStatus();

  EXPECT_TRUE(device_status_.storage_status().has_lifetime_estimation());
  EXPECT_TRUE(device_status_.storage_status().lifetime_estimation().has_slc());
  EXPECT_TRUE(device_status_.storage_status().lifetime_estimation().has_mlc());
  EXPECT_EQ(est.slc(),
            device_status_.storage_status().lifetime_estimation().slc());
  EXPECT_EQ(est.mlc(),
            device_status_.storage_status().lifetime_estimation().mlc());
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_FALSE(device_status_.storage_status().has_lifetime_estimation());
}

TEST_F(DeviceStatusCollectorTest, KioskAndroidReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));
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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

  const AccountId account_id(AccountId::FromUserEmail(kCrostiniUserEmail));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);
  testing_profile_->GetPrefs()->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaComponentVersion,
      kTerminaVmComponentVersion);
  testing_profile_->GetPrefs()->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart,
      kLastLaunchTimeWindowStartInJavaTime);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  EXPECT_EQ(kLastLaunchTimeWindowStartInJavaTime,
            session_status_.crostini_status()
                .last_launch_time_window_start_timestamp());
  EXPECT_EQ(kTerminaVmComponentVersion,
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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

  const AccountId account_id(AccountId::FromUserEmail(kCrostiniUserEmail));
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

TEST_F(DeviceStatusCollectorTest, CrostiniTerminaVmKernelVersionReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

  // Prerequisites for any Crostini reporting to take place:
  const AccountId account_id(AccountId::FromUserEmail(kCrostiniUserEmail));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);
  testing_profile_->GetPrefs()->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart,
      kLastLaunchTimeWindowStartInJavaTime);
  testing_profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                           true);

  // Set the kernel version to be reported in our cache:
  testing_profile_->GetPrefs()->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion,
      kTerminaVmKernelVersion);

  // Check that the kernel version is reported as to the feature flag default:
  GetStatus();
  EXPECT_TRUE(got_session_status_);
  EXPECT_EQ(kTerminaVmKernelVersion,
            session_status_.crostini_status().last_launch_vm_kernel_version());

  // Check that nothing is reported when the feature flag is disabled:
  scoped_feature_list_.InitAndDisableFeature(
      features::kCrostiniAdditionalEnterpriseReporting);
  GetStatus();
  EXPECT_TRUE(got_session_status_);
  EXPECT_TRUE(session_status_.crostini_status()
                  .last_launch_vm_kernel_version()
                  .empty());
}

TEST_F(DeviceStatusCollectorTest, CrostiniAppUsageReporting) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

  const AccountId account_id(AccountId::FromUserEmail(kCrostiniUserEmail));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);
  testing_profile_->GetPrefs()->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaComponentVersion,
      kTerminaVmComponentVersion);
  testing_profile_->GetPrefs()->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart,
      kLastLaunchTimeWindowStartInJavaTime);

  testing_profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                           true);
  scoped_feature_list_.InitAndEnableFeature(
      features::kCrostiniAdditionalEnterpriseReporting);

  const std::string desktop_file_id = "vim";
  const std::string package_id =
      "vim;2:8.0.0197-4+deb9u1;amd64;installed:debian-stable";

  vm_tools::apps::ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList(
          desktop_file_id, crostini::kCrostiniDefaultVmName,
          crostini::kCrostiniDefaultContainerName);
  app_list.mutable_apps(0)->set_package_id(package_id);

  crostini::CrostiniRegistryService* const registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          testing_profile_.get());
  registry_service->UpdateApplicationList(app_list);
  base::Time last_launch_time;
  EXPECT_TRUE(base::Time::FromString(kActualLastLaunchTimeFormatted,
                                     &last_launch_time));
  test_clock_.SetNow(last_launch_time);
  registry_service->SetClockForTesting(&test_clock_);
  registry_service->AppLaunched(crostini::CrostiniTestHelper::GenerateAppId(
      desktop_file_id, crostini::kCrostiniDefaultVmName,
      crostini::kCrostiniDefaultContainerName));

  GetStatus();
  EXPECT_TRUE(got_session_status_);

  EXPECT_EQ(2, session_status_.crostini_status().installed_apps_size());
  EXPECT_EQ(desktop_file_id,
            session_status_.crostini_status().installed_apps()[0].app_name());
  EXPECT_EQ(em::CROSTINI_APP_TYPE_INTERACTIVE,
            session_status_.crostini_status().installed_apps()[0].app_type());
  base::Time last_launch_time_coarsed;
  EXPECT_TRUE(base::Time::FromString(kLastLaunchTimeWindowStartFormatted,
                                     &last_launch_time_coarsed));
  EXPECT_EQ(kLastLaunchTimeWindowStartInJavaTime,
            session_status_.crostini_status()
                .installed_apps()[0]
                .last_launch_time_window_start_timestamp());
  EXPECT_EQ(
      "vim",
      session_status_.crostini_status().installed_apps()[0].package_name());
  EXPECT_EQ(
      "2:8.0.0197-4+deb9u1",
      session_status_.crostini_status().installed_apps()[0].package_version());
  EXPECT_EQ("Terminal",
            session_status_.crostini_status().installed_apps()[1].app_name());
  EXPECT_EQ(em::CROSTINI_APP_TYPE_TERMINAL,
            session_status_.crostini_status().installed_apps()[1].app_type());
  EXPECT_EQ(
      std::string(),
      session_status_.crostini_status().installed_apps()[1].package_name());
  EXPECT_EQ(
      std::string(),
      session_status_.crostini_status().installed_apps()[1].package_version());
  EXPECT_EQ(0, session_status_.crostini_status()
                   .installed_apps()[1]
                   .last_launch_time_window_start_timestamp());

  // In tests, GetUserDMToken returns the e-mail for easy verification.
  EXPECT_EQ(account_id.GetUserEmail(), session_status_.user_dm_token());
}

TEST_F(DeviceStatusCollectorTest,
       TerminalAppIsNotReportedIfCrostiniHasBeenRemoved) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

  const AccountId account_id(AccountId::FromUserEmail(kCrostiniUserEmail));
  MockRegularUserWithAffiliation(account_id, true);
  testing_profile_->GetPrefs()->SetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled, true);
  testing_profile_->GetPrefs()->SetString(
      crostini::prefs::kCrostiniLastLaunchTerminaComponentVersion,
      kTerminaVmComponentVersion);
  testing_profile_->GetPrefs()->SetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart,
      kLastLaunchTimeWindowStartInJavaTime);

  testing_profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                           false);
  scoped_feature_list_.InitAndEnableFeature(
      features::kCrostiniAdditionalEnterpriseReporting);

  GetStatus();
  EXPECT_TRUE(got_session_status_);

  // crostini::prefs::kCrostiniEnabled is set to false, but there
  // is general last launch information. This means Crostini has been
  // disabled after it has been used. We still report general last launch
  // information but do not want to jump into application reporting, because
  // the registry always has the terminal, even if Crostini has been
  // uninstalled.
  EXPECT_EQ(kLastLaunchTimeWindowStartInJavaTime,
            session_status_.crostini_status()
                .last_launch_time_window_start_timestamp());
  EXPECT_EQ(0, session_status_.crostini_status().installed_apps_size());
}

TEST_F(DeviceStatusCollectorTest, NoRegularUserReportingByDefault) {
  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

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
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetEmptyCrosHealthdData));

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
  RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                         base::BindRepeating(&GetEmptyCPUStatistics),
                         base::BindRepeating(&GetEmptyCPUTempInfo),
                         base::BindRepeating(&GetEmptyAndroidStatus),
                         base::BindRepeating(&GetFakeTpmStatus, kFakeTpmStatus),
                         base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
                         base::BindRepeating(&GetEmptyStatefulPartitionInfo),
                         base::BindRepeating(&GetEmptyCrosHealthdData));

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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceSessionStatus, true);
  GetStatus();
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, NoSessionStatusIfSessionReportingDisabled) {
  // Should not report session status if session status reporting is disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceSessionStatus, false);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceSessionStatus, true);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceSessionStatus, true);
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
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.has_os_update_status());
}

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatusUpToDate) {
  MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportOsUpdateStatus, true);

  const char* kRequiredPlatformVersions[] = {"1234", "1234.0", "1234.0.0"};

  for (size_t i = 0; i < base::size(kRequiredPlatformVersions); ++i) {
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
  MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportOsUpdateStatus, true);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1235");

  update_engine::StatusResult update_status;
  update_status.set_current_operation(update_engine::Operation::IDLE);

  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED,
            device_status_.os_update_status().update_status());

  const update_engine::Operation kUpdateEngineOps[] = {
      update_engine::Operation::DOWNLOADING,
      update_engine::Operation::VERIFYING,
      update_engine::Operation::FINALIZING,
  };

  for (size_t i = 0; i < base::size(kUpdateEngineOps); ++i) {
    update_status.set_current_operation(kUpdateEngineOps[i]);
    update_status.set_new_version("1235.1.2");
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

  update_status.set_current_operation(
      update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client_->PushLastStatus(update_status);
  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT,
            device_status_.os_update_status().update_status());
}

TEST_F(DeviceStatusCollectorTest, NoLastCheckedTimestampByDefault) {
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.os_update_status().has_last_checked_timestamp());
}

TEST_F(DeviceStatusCollectorTest, ReportLastCheckedTimestamp) {
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportOsUpdateStatus, true);

  // Check update multiple times, the timestamp stored in device status should
  // change accordingly.
  const int64 kLastCheckedTimes[] = {10, 20, 30};

  for (size_t i = 0; i < base::size(kLastCheckedTimes); ++i) {
    update_engine::StatusResult update_status;
    update_status.set_new_version(kDefaultPlatformVersion);
    update_status.set_last_checked_time(kLastCheckedTimes[i]);
    update_engine_client_->PushLastStatus(update_status);

    GetStatus();
    ASSERT_TRUE(device_status_.os_update_status().has_last_checked_timestamp());

    // The timestamp precision in UpdateEngine is in seconds, but the
    // DeviceStatusCollector is in milliseconds. Therefore, the number should be
    // multiplied by 1000 before validation.
    ASSERT_EQ(kLastCheckedTimes[i] * 1000,
              device_status_.os_update_status().last_checked_timestamp());
  }
}

TEST_F(DeviceStatusCollectorTest, NoLastRebootTimestampByDefault) {
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.os_update_status().has_last_reboot_timestamp());
}

TEST_F(DeviceStatusCollectorTest, ReportLastRebootTimestamp) {
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportOsUpdateStatus, true);

  GetStatus();
  ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());

  // No good way to inject specific last reboot timestamp of the test machine,
  // so just make sure UnixEpoch < RebootTime < Now.
  EXPECT_GT(device_status_.os_update_status().last_reboot_timestamp(),
            base::Time::UnixEpoch().ToJavaTime());
  EXPECT_LT(device_status_.os_update_status().last_reboot_timestamp(),
            base::Time::Now().ToJavaTime());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppByDefault) {
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_, false /* arc_kiosk */);

  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppWhenNotInKioskSession) {
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportRunningKioskApp, true);
  MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, ReportRunningKioskApp) {
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportRunningKioskApp, true);
  MockPlatformVersion(kDefaultPlatformVersion);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportRunningKioskApp, true);
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
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_sound_volume());

  // Try setting a custom volume value and check that it matches.
  const int kCustomVolume = 42;
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceHardwareStatus, true);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(kCustomVolume);
  GetStatus();
  EXPECT_EQ(kCustomVolume, device_status_.sound_volume());
}

TEST_F(DeviceStatusCollectorTest, TestStatefulPartitionInfo) {
  // Create a fake stateful partition info and populate it with some arbitrary
  // values.
  em::StatefulPartitionInfo fakeStatefulPartitionInfo;
  fakeStatefulPartitionInfo.set_available_space(350);
  fakeStatefulPartitionInfo.set_total_space(500);

  RestartStatusCollector(base::BindRepeating(&GetEmptyVolumeInfo),
                         base::BindRepeating(&GetEmptyCPUStatistics),
                         base::BindRepeating(&GetEmptyCPUTempInfo),
                         base::BindRepeating(&GetEmptyAndroidStatus),
                         base::BindRepeating(&GetEmptyTpmStatus),
                         base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
                         base::BindRepeating(&GetFakeStatefulPartitionInfo,
                                             fakeStatefulPartitionInfo),
                         base::BindRepeating(&GetEmptyCrosHealthdData));

  GetStatus();

  EXPECT_TRUE(device_status_.has_stateful_partition_info());
  EXPECT_EQ(fakeStatefulPartitionInfo.available_space(),
            device_status_.stateful_partition_info().available_space());
  EXPECT_EQ(fakeStatefulPartitionInfo.total_space(),
            device_status_.stateful_partition_info().total_space());
}

TEST_F(DeviceStatusCollectorTest, TestCrosHealthdInfo) {
  // Create a fake response from cros_healthd and populate it with some
  // arbitrary values.

  // Cached VPD test values.
  constexpr char kFakeSkuNumber[] = "fake_sku_number";

  // Storage test values.
  constexpr char kFakeStoragePath[] = "fake_storage_path";
  constexpr int kFakeStorageSize = 123;
  constexpr char kFakeStorageType[] = "fake_storage_type";
  constexpr uint8_t kFakeStorageManfid = 2;
  constexpr char kFakeStorageName[] = "fake_storage_name";
  constexpr int kFakeStorageSerial = 789;

  // Battery test values.
  constexpr int kFakeCycleCount = 3;
  constexpr int kExpectedVoltageNow = 12574;                        // (mV)
  constexpr double kFakeVoltageNow = kExpectedVoltageNow / 1000.0;  // (V)
  constexpr char kFakeBatteryVendor[] = "fake_battery_vendor";
  constexpr char kFakeBatterySerial[] = "fake_battery_serial";
  constexpr int kExpectedChargeFullDesign = 5275;  // (mAh)
  constexpr double kFakeChargeFullDesign =
      kExpectedChargeFullDesign / 1000.0;                           // (Ah)
  constexpr int kExpectedChargeFull = 5292;                         // (mAh)
  constexpr double kFakeChargeFull = kExpectedChargeFull / 1000.0;  // (Ah)
  constexpr int kExpectedVoltageMinDesign = 11550;                  // (mV)
  constexpr double kFakeVoltageMinDesign =
      kExpectedVoltageMinDesign / 1000.0;  // (V)
  constexpr int kFakeManufactureDateSmart = 19718;
  constexpr int kFakeTemperatureSmart = 3004;
  constexpr char kFakeBatteryModel[] = "fake_battery_model";
  constexpr int kExpectedChargeNow = 5281;                        // (mAh)
  constexpr double kFakeChargeNow = kExpectedChargeNow / 1000.0;  // (Ah)

  // CPU Temperature test values.
  constexpr char kFakeCpuLabel[] = "fake_cpu_label";
  constexpr int kFakeCpuTemp = 91832;
  constexpr int kFakeCpuTimestamp = 912;

  chromeos::cros_healthd::mojom::BatteryInfo battery_info(
      kFakeCycleCount, kFakeVoltageNow, kFakeBatteryVendor, kFakeBatterySerial,
      kFakeChargeFullDesign, kFakeChargeFull, kFakeVoltageMinDesign,
      kFakeManufactureDateSmart, kFakeTemperatureSmart, kFakeBatteryModel,
      kFakeChargeNow);
  chromeos::cros_healthd::mojom::CachedVpdInfo cached_vpd_info(kFakeSkuNumber);
  chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info(
      kFakeStoragePath, kFakeStorageSize, kFakeStorageType, kFakeStorageManfid,
      kFakeStorageName, kFakeStorageSerial);

  // Create a fake sample to test with.
  em::CPUTempInfo fake_cpu_temp_sample;
  fake_cpu_temp_sample.set_cpu_label(kFakeCpuLabel);
  fake_cpu_temp_sample.set_cpu_temp(kFakeCpuTemp);
  fake_cpu_temp_sample.set_timestamp(kFakeCpuTimestamp);
  em::BatterySample fake_battery_sample;
  // Convert from V to mV.
  fake_battery_sample.set_voltage(kExpectedVoltageNow);
  // Convert from Ah to mAh.
  fake_battery_sample.set_remaining_capacity(kExpectedChargeNow);
  fake_battery_sample.set_temperature(kFakeTemperatureSmart);

  RestartStatusCollector(
      base::BindRepeating(&GetEmptyVolumeInfo),
      base::BindRepeating(&GetEmptyCPUStatistics),
      base::BindRepeating(&GetEmptyCPUTempInfo),
      base::BindRepeating(&GetEmptyAndroidStatus),
      base::BindRepeating(&GetEmptyTpmStatus),
      base::BindRepeating(&GetEmptyEMMCLifetimeEstimation),
      base::BindRepeating(&GetEmptyStatefulPartitionInfo),
      base::BindRepeating(&GetFakeCrosHealthdData, battery_info,
                          cached_vpd_info, storage_info, fake_cpu_temp_sample,
                          fake_battery_sample));

  // If neither kReportDevicePowerStatus nor kReportDeviceStorageStatus are set,
  // expect that the data from cros_healthd isn't present in the protobuf.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDevicePowerStatus, false);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceStorageStatus, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_power_status());
  EXPECT_FALSE(device_status_.has_storage_status());
  EXPECT_FALSE(device_status_.has_system_status());

  // When kReportDevicePowerStatus and kReportDeviceStorageStatus are set,
  // expect the protobuf to have the data from cros_healthd.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDevicePowerStatus, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceStorageStatus, true);
  GetStatus();

  // Check that the CPU temperature samples are stored correctly.
  ASSERT_EQ(device_status_.cpu_temp_infos_size(), 1);
  const auto& cpu_sample = device_status_.cpu_temp_infos(0);
  EXPECT_EQ(cpu_sample.cpu_label(), kFakeCpuLabel);
  EXPECT_EQ(cpu_sample.cpu_temp(), kFakeCpuTemp);
  EXPECT_EQ(cpu_sample.timestamp(), kFakeCpuTimestamp);

  // Verify the battery data.
  ASSERT_TRUE(device_status_.has_power_status());
  ASSERT_EQ(device_status_.power_status().batteries_size(), 1);
  const auto& battery = device_status_.power_status().batteries(0);
  EXPECT_EQ(battery.serial(), kFakeBatterySerial);
  EXPECT_EQ(battery.manufacturer(), kFakeBatteryVendor);
  EXPECT_EQ(battery.design_capacity(), kExpectedChargeFullDesign);
  EXPECT_EQ(battery.full_charge_capacity(), kExpectedChargeFull);
  EXPECT_EQ(battery.cycle_count(), kFakeCycleCount);
  EXPECT_EQ(battery.design_min_voltage(), kExpectedVoltageMinDesign);
  EXPECT_EQ(battery.manufacture_date(), "2018-08-06");

  // Verify the battery sample data.
  ASSERT_EQ(battery.samples_size(), 1);
  const auto& battery_sample = battery.samples(0);
  EXPECT_EQ(battery_sample.voltage(), kExpectedVoltageNow);
  EXPECT_EQ(battery_sample.remaining_capacity(), kExpectedChargeNow);
  EXPECT_EQ(battery_sample.temperature(), kFakeTemperatureSmart);

  // Verify the storage data.
  ASSERT_TRUE(device_status_.has_storage_status());
  ASSERT_EQ(device_status_.storage_status().disks_size(), 1);
  const auto& disk = device_status_.storage_status().disks(0);
  EXPECT_EQ(disk.size(), kFakeStorageSize);
  EXPECT_EQ(disk.type(), kFakeStorageType);
  EXPECT_EQ(disk.manufacturer(), base::NumberToString(kFakeStorageManfid));
  EXPECT_EQ(disk.model(), kFakeStorageName);
  EXPECT_EQ(disk.serial(), base::NumberToString(kFakeStorageSerial));

  // Verify the Cached VPD.
  ASSERT_TRUE(device_status_.has_system_status());
  EXPECT_EQ(device_status_.system_status().vpd_sku_number(), kFakeSkuNumber);
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
  bool visible;
};

// List of fake networks - primarily used to make sure that signal strength
// and connection state are properly populated in status reports. Note that
// by convention shill will not report a signal strength of 0 for a visible
// network, so we use 1 below.
static const FakeNetworkState kFakeNetworks[] = {
    {"offline", "/device/wifi", shill::kTypeWifi, 35, -85, shill::kStateOffline,
     em::NetworkState::OFFLINE, "", "", true},
    {"ethernet", "/device/ethernet", shill::kTypeEthernet, 0, 0,
     shill::kStateOnline, em::NetworkState::ONLINE, "192.168.0.1", "8.8.8.8",
     true},
    {"wifi", "/device/wifi", shill::kTypeWifi, 23, -97,
     shill::kStateNoConnectivity, em::NetworkState::PORTAL, "", "", true},
    {"idle", "/device/cellular1", shill::kTypeCellular, 0, 0, shill::kStateIdle,
     em::NetworkState::IDLE, "", "", true},
    {"not_visible", "/device/wifi", shill::kTypeWifi, 0, 0, shill::kStateIdle,
     em::NetworkState::IDLE, "", "", false},
    {"association", "/device/cellular1", shill::kTypeCellular, 0, 0,
     shill::kStateAssociation, em::NetworkState::ASSOCIATION, "", "", true},
    {"config", "/device/cellular1", shill::kTypeCellular, 0, 0,
     shill::kStateConfiguration, em::NetworkState::CONFIGURATION, "", "", true},
    // Set signal strength for this network to -20, but expected strength to 0
    // to test that we only report signal_strength for wifi connections.
    {"ready", "/device/cellular1", shill::kTypeCellular, -20, 0,
     shill::kStateReady, em::NetworkState::READY, "", "", true},
    {"failure", "/device/wifi", shill::kTypeWifi, 1, -119, shill::kStateFailure,
     em::NetworkState::FAILURE, "", "", true},
    {"activation-failure", "/device/cellular1", shill::kTypeCellular, 0, 0,
     shill::kStateActivationFailure, em::NetworkState::ACTIVATION_FAILURE, "",
     "", true},
    {"unknown", "", shill::kTypeWifi, 1, -119, shill::kStateIdle,
     em::NetworkState::IDLE, "", "", true},
};

static const FakeNetworkState kUnconfiguredNetwork = {
  "unconfigured", "/device/unconfigured", shill::kTypeWifi, 35, -85,
  shill::kStateOffline, em::NetworkState::OFFLINE, "", ""
};

class DeviceStatusCollectorNetworkInterfacesTest
    : public DeviceStatusCollectorTest {
 protected:
  void SetUp() override {
    DeviceStatusCollectorTest::SetUp();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        chromeos::kReportDeviceNetworkInterfaces, true);

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
      bool is_visible = fake_network.connection_status != shill::kStateIdle;
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
    ASSERT_EQ(base::size(kFakeNetworks), state_list.size());
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    DeviceStatusCollectorTest::TearDown();
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
      for (iface = device_status_.network_interfaces().begin();
           iface != device_status_.network_interfaces().end(); ++iface) {
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

    EXPECT_EQ(count, device_status_.network_interfaces_size());

    // Now make sure network state list is correct.
    EXPECT_EQ(base::size(kFakeNetworks),
              static_cast<size_t>(device_status_.network_states_size()));
    for (const FakeNetworkState& state : kFakeNetworks) {
      bool found_match = false;
      for (const em::NetworkState& proto_state :
           device_status_.network_states()) {
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
  EXPECT_LT(0, device_status_.network_interfaces_size());
  EXPECT_EQ(0, device_status_.network_states_size());
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, NetworkInterfaces) {
  // Mock that we are in kiosk mode so we report network state.
  status_collector_->set_kiosk_account(
      std::make_unique<policy::DeviceLocalAccount>(
          fake_kiosk_device_local_account_));

  // Interfaces should be reported by default.
  GetStatus();
  EXPECT_LT(0, device_status_.network_interfaces_size());
  EXPECT_LT(0, device_status_.network_states_size());

  // No interfaces should be reported if the policy is off.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceNetworkInterfaces, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.network_interfaces_size());
  EXPECT_EQ(0, device_status_.network_states_size());

  // Switch the policy on and verify the interface list is present.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceNetworkInterfaces, true);
  GetStatus();

  VerifyNetworkReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, ReportIfPublicSession) {
  // Report netowork state for public accounts.
  user_manager_->CreatePublicAccountUser(
      AccountId::FromUserEmail(kPublicAccountId));
  EXPECT_CALL(*user_manager_, IsLoggedInAsPublicAccount())
      .WillRepeatedly(Return(true));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kReportDeviceNetworkInterfaces, true);
  GetStatus();
  VerifyNetworkReporting();
}

}  // namespace policy
