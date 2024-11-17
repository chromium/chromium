// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publisher_host.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"
#include "chrome/browser/ash/policy/status_collector/enterprise_activity_storage.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/upload_list/upload_list.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
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
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"

namespace policy {

namespace {

using ::ash::disks::DiskMountManager;
using ::base::test::ScopedChromeOSVersionInfo;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;
namespace em = ::enterprise_management;
namespace cros_healthd = ::ash::cros_healthd::mojom;

// Test values for cros_healthd:
// Battery test values:
constexpr int kFakeBatteryCycleCount = 3;
constexpr int kExpectedBatteryVoltageNow = 12574;  // (mV)
constexpr double kFakeBatteryVoltageNow =
    kExpectedBatteryVoltageNow / 1000.0;  // (V)
constexpr char kFakeBatteryVendor[] = "fake_battery_vendor";
constexpr char kFakeBatterySerial[] = "fake_battery_serial";
constexpr int kExpectedBatteryChargeFullDesign = 5275;  // (mAh)
constexpr double kFakeBatteryChargeFullDesign =
    kExpectedBatteryChargeFullDesign / 1000.0;    // (Ah)
constexpr int kExpectedBatteryChargeFull = 5292;  // (mAh)
constexpr double kFakeBatteryChargeFull =
    kExpectedBatteryChargeFull / 1000.0;                 // (Ah)
constexpr int kExpectedBatteryVoltageMinDesign = 11550;  // (mV)
constexpr double kFakeBatteryVoltageMinDesign =
    kExpectedBatteryVoltageMinDesign / 1000.0;  // (V)
constexpr char kFakeSmartBatteryManufactureDate[] = "2018-08-06";
constexpr int kFakeSmartBatteryTemperature = 3004;
constexpr int kZeroCInDeciKelvin = 2731;
constexpr char kFakeBatteryModel[] = "fake_battery_model";
constexpr int kExpectedBatteryChargeNow = 5281;  // (mAh)
constexpr double kFakeBatteryChargeNow =
    kExpectedBatteryChargeNow / 1000.0;            // (Ah)
constexpr int kExpectedBatteryCurrentNow = 87659;  // (mA)
constexpr double kFakeBatteryCurrentNow =
    kExpectedBatteryCurrentNow / 1000.0;  // (A)
constexpr char kFakeBatteryTechnology[] = "fake_battery_technology";
constexpr char kFakeBatteryStatus[] = "fake_battery_status";
// System test values:
constexpr char kFakeDmiInfoBiosVendor[] = "DMI Bios Vendor";
constexpr char kFakeDmiInfoBiosVersion[] = "Google_BoardName.12200.68.0";
constexpr char kFakeDmiInfoBoardName[] = "DMI Board Name";
constexpr char kFakeDmiInfoBoardVendor[] = "DMI Board Vendor";
constexpr char kFakeDmiInfoBoardVersion[] = "rev1234";
constexpr uint64_t kFakeDmiInfoChassisType = 9;
constexpr char kFakeDmiInfoChassisVendor[] = "DMI Chassis Vendor";
constexpr char kFakeDmiInfoProductFamily[] = "DMI Product Family";
constexpr char kFakeDmiInfoProductName[] = "DMI Product Name";
constexpr char kFakeDmiInfoProductVersion[] = "DMI Product Version";
constexpr char kFakeDmiInfoSysVendor[] = "DMI System Vendor";
constexpr em::BootInfo::BootMethod kFakeOsInfoBootMethod =
    em::BootInfo::CROS_SECURE;
constexpr cros_healthd::BootMode kFakeOsInfoBootMode =
    cros_healthd::BootMode::kCrosSecure;
constexpr char kFakeOsInfoMarketingName[] =
    "Latitude 1234 Chromebook Enterprise";
constexpr char kFakeOsInfoOemName[] = "OsInfo OEM Name";
constexpr char kFakeOsInfoProductName[] = "OsInfo Code Name";
constexpr char kFakeOsVersionBuildNumber[] = "13544";
constexpr char kFakeOsVersionMilestone[] = "87";
constexpr char kFakeOsVersionPatchNumber[] = "59.0";
constexpr char kFakeOsVersionReleaseChannel[] = "stable-channel";
constexpr char kFakeVpdInfoFirstPowerDate[] = "2020-40";
constexpr char kFakeVpdInfoManufactureDate[] = "2019-01-01";
constexpr char kFakeVpdInfoRegion[] = "VpdInfo Region";
constexpr char kFakeVpdInfoSerialNumber[] = "8607G03EDF";
constexpr char kFakeVpdInfoSkuNumber[] = "ABCD&^A";
constexpr char kFakeVpdInfoSystemModelName[] = "XX ModelName 007 XY";
// CPU test values:
constexpr uint32_t kFakeNumTotalThreads = 8;
constexpr char kFakeModelName[] = "fake_cpu_model_name";
constexpr int32_t kFakeCpuTemperature = -189;
constexpr char kFakeCpuTemperatureLabel[] = "Fake CPU temperature";
constexpr cros_healthd::CpuArchitectureEnum kFakeMojoArchitecture =
    cros_healthd::CpuArchitectureEnum::kX86_64;
constexpr em::CpuInfo::Architecture kFakeProtoArchitecture =
    em::CpuInfo::X86_64;
constexpr bool kFakeKeylockerConfigurationState = false;
constexpr uint32_t kFakeMaxClockSpeed = 3400000;
constexpr uint32_t kFakeScalingMaxFrequency = 2700000;
constexpr uint32_t kFakeScalingCurFrequency = 2400000;
// Since this number is divided by the result of the sysconf(_SC_CLK_TCK)
// syscall, we need it to be 0 to avoid flaky tests,
constexpr uint64_t kFakeIdleTime = 0;
constexpr uint64_t kFakeUserTime = 789;
constexpr uint64_t kFakeSystemTime = 4680;
constexpr char kFakeCStateName[] = "fake_c_state_name";
constexpr uint64_t kFakeTimeInStateSinceLastBoot = 87;
// CPU Temperature test values:
constexpr int kFakeCpuTimestamp = 912;
// Storage test values:
constexpr char kFakeStoragePath[] = "fake_storage_path";
constexpr int kFakeStorageSize = 123;
constexpr char kFakeStorageType[] = "fake_storage_type";
constexpr uint8_t kFakeStorageManfid = 2;
constexpr char kFakeStorageName[] = "fake_storage_name";
constexpr int kFakeStorageSerial = 789;
constexpr uint64_t kFakeStorageBytesRead = 9070;
constexpr uint64_t kFakeStorageBytesWritten = 87653;
constexpr uint64_t kFakeStorageReadTimeSeconds = 23570;
constexpr uint64_t kFakeStorageWriteTimeSeconds = 5768;
constexpr uint64_t kFakeStorageIoTimeSeconds = 709;
constexpr uint64_t kFakeStorageDiscardTimeSeconds = 9869;
constexpr uint16_t kFakeOemid = 274;
constexpr uint64_t kFakePnm = 8321204;
constexpr uint8_t kFakePrv = 5;
constexpr uint64_t kFakeFwrev = 1704189236;
constexpr cros_healthd::StorageDevicePurpose kFakeMojoPurpose =
    cros_healthd::StorageDevicePurpose::kBootDevice;
constexpr em::DiskInfo::DevicePurpose kFakeProtoPurpose =
    em::DiskInfo::PURPOSE_BOOT;
// Timezone test values:
constexpr char kPosixTimezone[] = "MST7MDT,M3.2.0,M11.1.0";
constexpr char kTimezoneRegion[] = "America/Denver";
// Memory test values:
constexpr uint32_t kFakeTotalMemory = 1287312;
constexpr uint32_t kFakeFreeMemory = 981239;
constexpr uint32_t kFakeAvailableMemory = 98719321;
constexpr uint64_t kFakePageFaults = 896123761;
// Backlight test values:
constexpr char kFakeBacklightPath[] = "/sys/class/backlight/fake_backlight";
constexpr uint32_t kFakeMaxBrightness = 769;
constexpr uint32_t kFakeBrightness = 124;
// Fan test values:
constexpr uint32_t kFakeSpeedRpm = 1225;
// Stateful partition test values:
constexpr uint64_t kAvailableSpace = 777;
constexpr uint64_t kTotalSpace = 999;
constexpr char kFilesystem[] = "ext4";
constexpr char kMountSource[] = "/dev/mmcblk0p1";
// Bluetooth test values:
constexpr char kFakeBluetoothAdapterName[] = "Marty Byrde's BT Adapter";
constexpr char kFakeBluetoothAdapterAddress[] = "aa:bb:cc:dd:ee:ff";
constexpr bool kFakeBluetoothAdapterIsPowered = true;
constexpr uint32_t kFakeNumConnectedBluetoothDevices = 7;
// Tpm test values:
constexpr char kFakeTpmDidVid[] = "fake_tpm_did_vid";
// Bus Device test values:
constexpr uint8_t kFakeUnusedBusId = 1;
constexpr char kFakePciVendor[] = "pci_vendor";
constexpr char kFakePciProduct[] = "pci_product";
constexpr char kFakePciDriver[] = "pci_driver";
constexpr char kFakeUsbVendor[] = "usb_vendor";
constexpr char kFakeUsbProduct[] = "usb_product";
constexpr char kFakeUsbDriver0[] = "usb_driver_1";
constexpr char kFakeUsbDriver1[] = "usb_driver_2";
constexpr uint8_t kFakeUsbInterfaceNumber0 = 0;
constexpr uint8_t kFakeUsbInterfaceNumber1 = 1;

// Time delta representing 1 hour time interval.
constexpr base::TimeDelta kHour = base::Hours(1);

const char kKioskAccountId[] = "kiosk_user@localhost";
const char kWebKioskAccountId[] = "web_kiosk_user@localhost";
const char kKioskAppId[] = "kiosk_app_id";
const char kWebKioskAppUrl[] = "http://example.com";
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
const int64_t kLastLaunchTimeWindowStartInJavaTime = 1535760000000;
const char kDefaultPlatformVersion[] = "1234.0.0";

// Constants for crash reporting test cases:
const char kTestUploadId[] = "0123456789abcdef";
const char kTestLocalID[] = "fedcba9876543210";
const char kTestCauseKernel[] = "kernel";
const char kTestCauseEC[] = "embedded-controller";
const char kTestCauseOther[] = "other";

class TestingDeviceStatusCollectorOptions {
 public:
  TestingDeviceStatusCollectorOptions() = default;
  TestingDeviceStatusCollectorOptions(
      const TestingDeviceStatusCollectorOptions&) = delete;
  TestingDeviceStatusCollectorOptions& operator=(
      const TestingDeviceStatusCollectorOptions&) = delete;
  ~TestingDeviceStatusCollectorOptions() = default;

  DeviceStatusCollector::VolumeInfoFetcher volume_info_fetcher;
  DeviceStatusCollector::CPUStatisticsFetcher cpu_fetcher;
  DeviceStatusCollector::CPUTempFetcher cpu_temp_fetcher;
  StatusCollector::AndroidStatusFetcher android_status_fetcher;
  DeviceStatusCollector::EMMCLifetimeFetcher emmc_lifetime_fetcher;
  DeviceStatusCollector::StatefulPartitionInfoFetcher
      stateful_partition_info_fetcher;
  DeviceStatusCollector::GraphicsStatusFetcher graphics_status_fetcher;
  DeviceStatusCollector::CrashReportInfoFetcher crash_report_info_fetcher;
  std::unique_ptr<AppInfoGenerator> app_info_generator;
};

class TestingDeviceStatusCollector : public DeviceStatusCollector {
 public:
  // Note that that TpmStatusFetcher is null so the test exercises the
  // production logic with fake tpm manager and attestation clients.
  TestingDeviceStatusCollector(
      PrefService* pref_service,
      ReportingUserTracker* reporting_user_tracker,
      ash::system::StatisticsProvider* provider,
      ManagedSessionService* managed_session_service,
      std::unique_ptr<TestingDeviceStatusCollectorOptions> options,
      base::SimpleTestClock* clock)
      : DeviceStatusCollector(pref_service,
                              reporting_user_tracker,
                              provider,
                              managed_session_service,
                              options->volume_info_fetcher,
                              options->cpu_fetcher,
                              options->cpu_temp_fetcher,
                              options->android_status_fetcher,
                              DeviceStatusCollector::TpmStatusFetcher(),
                              options->emmc_lifetime_fetcher,
                              options->stateful_partition_info_fetcher,
                              options->graphics_status_fetcher,
                              options->crash_report_info_fetcher,
                              clock),
        test_clock_(*clock) {
    // Set the baseline time to a fixed value (1 hour after day start) to
    // prevent test flakiness due to a single activity period spanning two days.
    test_clock_->SetNow(base::Time::Now().LocalMidnight() + kHour);
  }

  void Simulate(ui::IdleState* states, int len) {
    for (int i = 0; i < len; i++) {
      test_clock_->Advance(DeviceStatusCollector::kIdlePollInterval);
      ProcessIdleState(states[i]);
    }
  }

  void set_max_stored_past_activity_interval(base::TimeDelta value) {
    max_stored_past_activity_interval_ = value;
  }

  void set_max_stored_future_activity_interval(base::TimeDelta value) {
    max_stored_future_activity_interval_ = value;
  }

  void set_kiosk_account(std::unique_ptr<DeviceLocalAccount> account) {
    kiosk_account_ = std::move(account);
  }

  std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo()
      override {
    if (kiosk_account_) {
      return std::make_unique<DeviceLocalAccount>(*kiosk_account_);
    }
    return nullptr;
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
    SampleCpuUsage();
    SampleMemoryUsage();
    content::RunAllTasksUntilIdle();
  }

 protected:
  void CheckIdleState() override {
    // This should never be called in testing, as it results in a dbus call.
    ADD_FAILURE();
  }

 private:
  const raw_ref<base::SimpleTestClock> test_clock_;

  std::unique_ptr<DeviceLocalAccount> kiosk_account_;
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

void CallAndroidStatusReceiver(StatusCollector::AndroidStatusReceiver receiver,
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

bool SettingEnabled(const std::string& path) {
  if (!ash::CrosSettings::Get()) {
    return false;
  }
  bool setting;
  if (!ash::CrosSettings::Get()->GetBoolean(path, &setting)) {
    setting = false;
  }
  return setting;
}

cros_healthd::BatteryResultPtr CreateBatteryResult() {
  return cros_healthd::BatteryResult::NewBatteryInfo(
      cros_healthd::BatteryInfo::New(
          kFakeBatteryCycleCount, kFakeBatteryVoltageNow, kFakeBatteryVendor,
          kFakeBatterySerial, kFakeBatteryChargeFullDesign,
          kFakeBatteryChargeFull, kFakeBatteryVoltageMinDesign,
          kFakeBatteryModel, kFakeBatteryChargeNow, kFakeBatteryCurrentNow,
          kFakeBatteryTechnology, kFakeBatteryStatus,
          kFakeSmartBatteryManufactureDate,
          cros_healthd::NullableUint64::New(kFakeSmartBatteryTemperature)));
}

cros_healthd::BatteryResultPtr CreateEmptyBatteryResult() {
  return cros_healthd::BatteryResult::NewBatteryInfo(
      cros_healthd::BatteryInfoPtr());
}

cros_healthd::NonRemovableBlockDeviceResultPtr CreateBlockDeviceResult() {
  std::vector<cros_healthd::NonRemovableBlockDeviceInfoPtr> storage_vector;
  storage_vector.push_back(cros_healthd::NonRemovableBlockDeviceInfo::New(
      kFakeStorageBytesRead, kFakeStorageBytesWritten,
      kFakeStorageReadTimeSeconds, kFakeStorageWriteTimeSeconds,
      kFakeStorageIoTimeSeconds,
      cros_healthd::NullableUint64::New(kFakeStorageDiscardTimeSeconds),
      cros_healthd::BlockDeviceVendor::NewEmmcOemid(kFakeOemid),
      cros_healthd::BlockDeviceProduct::NewEmmcPnm(kFakePnm),
      cros_healthd::BlockDeviceRevision::NewEmmcPrv(kFakePrv), kFakeStorageName,
      kFakeStorageSize,
      cros_healthd::BlockDeviceFirmware::NewEmmcFwrev(kFakeFwrev),
      kFakeStorageType, kFakeMojoPurpose, kFakeStoragePath, kFakeStorageManfid,
      kFakeStorageSerial));
  return cros_healthd::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
      std::move(storage_vector));
}

cros_healthd::SystemResultPtr CreateSystemResult() {
  return cros_healthd::SystemResult::NewSystemInfo(
      cros_healthd::SystemInfo::New(
          cros_healthd::OsInfo::New(
              kFakeOsInfoProductName, kFakeOsInfoMarketingName,
              cros_healthd::OsVersion::New(
                  kFakeOsVersionMilestone, kFakeOsVersionBuildNumber,
                  kFakeOsVersionPatchNumber, kFakeOsVersionReleaseChannel),
              kFakeOsInfoBootMode, kFakeOsInfoOemName),
          cros_healthd::VpdInfo::New(
              kFakeVpdInfoSerialNumber, kFakeVpdInfoRegion,
              kFakeVpdInfoManufactureDate, kFakeVpdInfoFirstPowerDate,
              kFakeVpdInfoSkuNumber, kFakeVpdInfoSystemModelName),
          cros_healthd::DmiInfo::New(
              kFakeDmiInfoBiosVendor, kFakeDmiInfoBiosVersion,
              kFakeDmiInfoBoardName, kFakeDmiInfoBoardVendor,
              kFakeDmiInfoBoardVersion, kFakeDmiInfoChassisVendor,
              cros_healthd::NullableUint64::New(kFakeDmiInfoChassisType),
              kFakeDmiInfoProductFamily, kFakeDmiInfoProductName,
              kFakeDmiInfoProductVersion, kFakeDmiInfoSysVendor)));
}

std::vector<cros_healthd::CpuCStateInfoPtr> CreateCStateInfo() {
  std::vector<cros_healthd::CpuCStateInfoPtr> c_states;
  c_states.push_back(cros_healthd::CpuCStateInfo::New(
      kFakeCStateName, kFakeTimeInStateSinceLastBoot));
  return c_states;
}

std::vector<cros_healthd::LogicalCpuInfoPtr> CreateLogicalCpu() {
  std::vector<cros_healthd::LogicalCpuInfoPtr> logical_cpus;
  logical_cpus.push_back(cros_healthd::LogicalCpuInfo::New(
      kFakeMaxClockSpeed, kFakeScalingMaxFrequency, kFakeScalingCurFrequency,
      kFakeUserTime, kFakeSystemTime, kFakeIdleTime, CreateCStateInfo()));
  return logical_cpus;
}

std::vector<cros_healthd::PhysicalCpuInfoPtr> CreatePhysicalCpu() {
  std::vector<cros_healthd::PhysicalCpuInfoPtr> physical_cpus;
  physical_cpus.push_back(
      cros_healthd::PhysicalCpuInfo::New(kFakeModelName, CreateLogicalCpu()));
  return physical_cpus;
}

std::vector<cros_healthd::CpuTemperatureChannelPtr> CreateTemperatureChannel() {
  std::vector<cros_healthd::CpuTemperatureChannelPtr> cpu_temps;
  cpu_temps.push_back(cros_healthd::CpuTemperatureChannel::New(
      kFakeCpuTemperatureLabel, kFakeCpuTemperature));
  return cpu_temps;
}

cros_healthd::KeylockerInfoPtr CreateKeylockerInfo() {
  return cros_healthd::KeylockerInfo::New(kFakeKeylockerConfigurationState);
}

cros_healthd::CpuResultPtr CreateCpuResult() {
  return cros_healthd::CpuResult::NewCpuInfo(cros_healthd::CpuInfo::New(
      kFakeNumTotalThreads, kFakeMojoArchitecture, CreatePhysicalCpu(),
      CreateTemperatureChannel(), CreateKeylockerInfo()));
}

cros_healthd::TimezoneResultPtr CreateTimezoneResult() {
  return cros_healthd::TimezoneResult::NewTimezoneInfo(
      cros_healthd::TimezoneInfo::New(kPosixTimezone, kTimezoneRegion));
}

cros_healthd::MemoryResultPtr CreateMemoryResult() {
  return cros_healthd::MemoryResult::NewMemoryInfo(
      cros_healthd::MemoryInfo::New(kFakeTotalMemory, kFakeFreeMemory,
                                    kFakeAvailableMemory, kFakePageFaults));
}

cros_healthd::BacklightResultPtr CreateBacklightResult() {
  std::vector<cros_healthd::BacklightInfoPtr> backlight_vector;
  backlight_vector.push_back(cros_healthd::BacklightInfo::New(
      kFakeBacklightPath, kFakeMaxBrightness, kFakeBrightness));
  return cros_healthd::BacklightResult::NewBacklightInfo(
      std::move(backlight_vector));
}

cros_healthd::BacklightResultPtr CreateEmptyBacklightResult() {
  std::vector<cros_healthd::BacklightInfoPtr> backlight_vector;
  return cros_healthd::BacklightResult::NewBacklightInfo(
      std::move(backlight_vector));
}

cros_healthd::FanResultPtr CreateFanResult() {
  std::vector<cros_healthd::FanInfoPtr> fan_vector;
  fan_vector.push_back(cros_healthd::FanInfo::New(kFakeSpeedRpm));
  return cros_healthd::FanResult::NewFanInfo(std::move(fan_vector));
}

cros_healthd::StatefulPartitionResultPtr CreateStatefulPartitionResult() {
  return cros_healthd::StatefulPartitionResult::NewPartitionInfo(
      cros_healthd::StatefulPartitionInfo::New(kAvailableSpace, kTotalSpace,
                                               kFilesystem, kMountSource));
}

cros_healthd::FanResultPtr CreateEmptyFanResult() {
  std::vector<cros_healthd::FanInfoPtr> fan_vector;
  return cros_healthd::FanResult::NewFanInfo(std::move(fan_vector));
}

cros_healthd::BluetoothResultPtr CreateBluetoothResult() {
  std::vector<cros_healthd::BluetoothAdapterInfoPtr> adapter_info;
  adapter_info.push_back(cros_healthd::BluetoothAdapterInfo::New(
      kFakeBluetoothAdapterName, kFakeBluetoothAdapterAddress,
      kFakeBluetoothAdapterIsPowered, kFakeNumConnectedBluetoothDevices));
  return cros_healthd::BluetoothResult::NewBluetoothAdapterInfo(
      std::move(adapter_info));
}

cros_healthd::TpmResultPtr CreateTpmResult() {
  return cros_healthd::TpmResult::NewTpmInfo(cros_healthd::TpmInfo::New(
      cros_healthd::TpmVersion::New(), cros_healthd::TpmStatus::New(),
      cros_healthd::TpmDictionaryAttack::New(),
      cros_healthd::TpmAttestation::New(),
      cros_healthd::TpmSupportedFeatures::New(), kFakeTpmDidVid));
}

cros_healthd::TpmResultPtr CreatePartialTpmResult() {
  return cros_healthd::TpmResult::NewTpmInfo(cros_healthd::TpmInfo::New(
      cros_healthd::TpmVersion::New(), cros_healthd::TpmStatus::New(),
      cros_healthd::TpmDictionaryAttack::New(),
      cros_healthd::TpmAttestation::New(),
      cros_healthd::TpmSupportedFeatures::New(), std::optional<std::string>()));
}

cros_healthd::BusResultPtr CreateBusResult() {
  std::vector<cros_healthd::BusDevicePtr> bus_devices;
  // pci bus device
  cros_healthd::PciBusInfoPtr pci_bus_info;
  pci_bus_info = cros_healthd::PciBusInfo::New(kFakeUnusedBusId,  // class_id
                                               kFakeUnusedBusId,  // subclass_id
                                               kFakeUnusedBusId,  // prog_if_id
                                               kFakeUnusedBusId,  // vendor_id
                                               kFakeUnusedBusId,  // device_id
                                               kFakePciDriver     // driver
  );
  bus_devices.push_back(cros_healthd::BusDevice::New(
      kFakePciVendor,                                     // vendor_name
      kFakePciProduct,                                    // product_name
      cros_healthd::BusDeviceClass::kEthernetController,  // device_class
      cros_healthd::BusInfo::NewPciBusInfo(std::move(pci_bus_info))));

  // usb bus device
  std::vector<cros_healthd::UsbBusInterfaceInfoPtr> usb_interfaces;
  usb_interfaces.push_back(cros_healthd::UsbBusInterfaceInfo::New(
      kFakeUsbInterfaceNumber0,  // interface_number
      kFakeUnusedBusId,          // class_id
      kFakeUnusedBusId,          // subclass_id
      kFakeUnusedBusId,          // protocol_id
      kFakeUsbDriver0            // driver
      ));
  usb_interfaces.push_back(cros_healthd::UsbBusInterfaceInfo::New(
      kFakeUsbInterfaceNumber1,  // interface_number
      kFakeUnusedBusId,          // class_id
      kFakeUnusedBusId,          // subclass_id
      kFakeUnusedBusId,          // protocol_id
      kFakeUsbDriver1            // driver
      ));
  cros_healthd::UsbBusInfoPtr usb_bus_info;
  usb_bus_info = cros_healthd::UsbBusInfo::New(kFakeUnusedBusId,  // class_id
                                               kFakeUnusedBusId,  // subclass_id
                                               kFakeUnusedBusId,  // protocol_id
                                               kFakeUnusedBusId,  // vendor_id
                                               kFakeUnusedBusId,  // product_id
                                               std::move(usb_interfaces));
  bus_devices.push_back(cros_healthd::BusDevice::New(
      kFakeUsbVendor,                                     // vendor_name
      kFakeUsbProduct,                                    // product_name
      cros_healthd::BusDeviceClass::kWirelessController,  // device_class
      cros_healthd::BusInfo::NewUsbBusInfo(std::move(usb_bus_info))));
  return cros_healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

// Set fake CrosHealthdData populated with arbitrary values based on enabled
// policies.
void SetFakeCrosHealthdData() {
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  cros_healthd::TelemetryInfo fake_info;
  // Always gather system result.
  telemetry_info->system_result = CreateSystemResult();
  if (SettingEnabled(ash::kReportDevicePowerStatus)) {
    telemetry_info->battery_result = CreateBatteryResult();
  }
  if (SettingEnabled(ash::kReportDeviceStorageStatus)) {
    telemetry_info->block_device_result = CreateBlockDeviceResult();
  }
  if (SettingEnabled(ash::kReportDeviceCpuInfo)) {
    telemetry_info->cpu_result = CreateCpuResult();
  }
  if (SettingEnabled(ash::kReportDeviceTimezoneInfo)) {
    telemetry_info->timezone_result = CreateTimezoneResult();
  }
  if (SettingEnabled(ash::kReportDeviceMemoryInfo)) {
    telemetry_info->memory_result = CreateMemoryResult();
  }
  if (SettingEnabled(ash::kReportDeviceBacklightInfo)) {
    telemetry_info->backlight_result = CreateBacklightResult();
  }
  if (SettingEnabled(ash::kReportDeviceFanInfo)) {
    telemetry_info->fan_result = CreateFanResult();
  }
  if (SettingEnabled(ash::kReportDeviceStorageStatus)) {
    telemetry_info->stateful_partition_result = CreateStatefulPartitionResult();
  }
  if (SettingEnabled(ash::kReportDeviceBluetoothInfo)) {
    telemetry_info->bluetooth_result = CreateBluetoothResult();
  }
  if (SettingEnabled(ash::kReportDeviceVersionInfo)) {
    telemetry_info->tpm_result = CreateTpmResult();
  }
  if (SettingEnabled(ash::kReportDeviceNetworkConfiguration)) {
    telemetry_info->bus_result = CreateBusResult();
  }

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
}

void GetEmptyGraphicsStatus(
    DeviceStatusCollector::GraphicsStatusReceiver receiver) {
  std::move(receiver).Run(em::GraphicsStatus());
}

void GetFakeGraphicsStatus(
    const em::GraphicsStatus& value,
    DeviceStatusCollector::GraphicsStatusReceiver receiver) {
  std::move(receiver).Run(value);
}

void GetEmptyCrashReportInfo(
    DeviceStatusCollector::CrashReportInfoReceiver receiver) {
  std::vector<em::CrashReportInfo> crash_report_infos;
  std::move(receiver).Run(crash_report_infos);
}

void GetFakeCrashReportInfo(
    const std::vector<em::CrashReportInfo>& crash_report_infos,
    DeviceStatusCollector::CrashReportInfoReceiver receiver) {
  std::move(receiver).Run(crash_report_infos);
}

}  // namespace

// Though it is a unit test, this test is linked with browser_tests so that it
// runs in a separate process. The intention is to avoid overriding the timezone
// environment variable for other tests.
class DeviceStatusCollectorTest : public testing::Test {
 public:
  // TODO(b/216186861) Default all policies to false for each unit test
  DeviceStatusCollectorTest()
      : user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        reporting_user_tracker_(std::make_unique<ReportingUserTracker>(
            user_manager::UserManager::Get())),
        got_session_status_(false),
        fake_kiosk_device_local_account_(
            DeviceLocalAccountType::kKioskApp,
            DeviceLocalAccount::EphemeralMode::kUnset,
            kKioskAccountId,
            kKioskAppId,
            std::string() /* kiosk_app_update_url */),
        fake_web_kiosk_app_basic_info_(kWebKioskAppUrl,
                                       std::string() /* title */,
                                       std::string() /* icon_url */),
        fake_web_kiosk_device_local_account_(
            DeviceLocalAccount::EphemeralMode::kUnset,
            fake_web_kiosk_app_basic_info_,
            kWebKioskAccountId),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        crash_dumps_dir_override_(chrome::DIR_CRASH_DUMPS) {
    scoped_stub_install_attributes_.Get()->SetCloudManaged("managed.com",
                                                           "device_id");

    // Ensure mojo is started, otherwise browser context keyed services that
    // rely on mojo will explode.
    mojo::core::Init();
    fake_service_manager_ =
        std::make_unique<::ash::mojo_service_manager::FakeMojoServiceManager>();

    // Although this is really a unit test which runs in the browser_tests
    // binary, it doesn't get the unit setup which normally happens in the unit
    // test binary.
    ChromeUnitTestSuite::InitializeProviders();
    ChromeUnitTestSuite::InitializeResourceBundle();

    content::SetContentClient(&content_client_);
    content::SetBrowserClientForTesting(&browser_content_client_);

    // Run this test with a well-known timezone so that
    // `base::Time::LocalMidnight()` returns the same values on all machines.
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("TZ", "UTC");

    // Initialize our mock mounted disk volumes.
    std::unique_ptr<ash::disks::MockDiskMountManager> mock_disk_mount_manager =
        std::make_unique<ash::disks::MockDiskMountManager>();
    AddMountPoint("/mount/volume1");
    AddMountPoint("/mount/volume2");
    EXPECT_CALL(*mock_disk_mount_manager, mount_points())
        .WillRepeatedly(ReturnRef(mount_point_map_));

    // Setup a fake file system that should show up in mount points.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        base::FilePath(kExternalMountPoint));

    // Just verify that we are properly setting the mount points.
    std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
    storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
        &external_mount_points);
    EXPECT_THAT(external_mount_points, Not(IsEmpty()));

    // DiskMountManager takes ownership of the MockDiskMountManager.
    DiskMountManager::InitializeForTesting(mock_disk_mount_manager.release());
    TestingDeviceStatusCollector::RegisterProfilePrefs(
        profile_pref_service_.registry());

    // Use FakeUpdateEngineClient.
    update_engine_client_ = ash::UpdateEngineClient::InitializeFakeForTest();

    ash::UserDataAuthClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
    ash::AttestationClient::InitializeFake();
    chromeos::TpmManagerClient::InitializeFake();
    ash::LoginState::Initialize();
    ash::cros_healthd::FakeCrosHealthd::Initialize();
    ash::FakeSpacedClient::InitializeFake();

    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
  }

  DeviceStatusCollectorTest(const DeviceStatusCollectorTest&) = delete;
  DeviceStatusCollectorTest& operator=(const DeviceStatusCollectorTest&) =
      delete;

  ~DeviceStatusCollectorTest() override {
    ash::SeneschalClient::Shutdown();
    kiosk_chrome_app_manager_.reset();
    // |testing_profile_| must be destroyed while ConciergeClient is alive.
    testing_profile_.reset();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::LoginState::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
    ash::AttestationClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::UserDataAuthClient::Shutdown();
    ash::CrasAudioHandler::Shutdown();
    ash::UpdateEngineClient::Shutdown();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    ash::FakeSpacedClient::Shutdown();

    // Finish pending tasks.
    content::RunAllTasksUntilIdle();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    DiskMountManager::Shutdown();
  }

  void SetUp() override {
    // Disable network interface reporting since it requires additional setup.
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkStatus, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkConfiguration, false);
    managed_session_service_ = std::make_unique<ManagedSessionService>();
    RestartStatusCollector();
  }

  void TearDown() override {
    status_collector_.reset();
    managed_session_service_.reset();
  }

 protected:
  ash::FakeChromeUserManager* GetFakeChromeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void AddMountPoint(const std::string& mount_point) {
    mount_point_map_.insert(
        {mount_point, mount_point, ash::MountType::kDevice});
  }

  virtual void RestartStatusCollector(
      std::unique_ptr<TestingDeviceStatusCollectorOptions> options) {
    status_collector_ = std::make_unique<TestingDeviceStatusCollector>(
        GetFakeChromeUserManager()->GetLocalState(),
        reporting_user_tracker_.get(), &fake_statistics_provider_,
        managed_session_service_.get(), std::move(options), &test_clock_);
  }

  void DisableDefaultSettings() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceVersionInfo, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceActivityTimes, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceBootMode, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceUsers, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceSessionStatus, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceAudioStatus, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkConfiguration, false);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkStatus, false);
  }

  void RestartStatusCollector() {
    RestartStatusCollector(CreateEmptyDeviceStatusCollectorOptions());
  }

  void WriteUploadLog(const std::string& log_data) {
    ASSERT_TRUE(base::WriteFile(log_path(), log_data));
  }

  base::FilePath log_path() {
    base::FilePath crash_dir_path;
    base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
    return crash_dir_path.AppendASCII("uploads.log");
  }

  std::unique_ptr<TestingDeviceStatusCollectorOptions>
  CreateEmptyDeviceStatusCollectorOptions() {
    auto options = std::make_unique<TestingDeviceStatusCollectorOptions>();
    options->volume_info_fetcher = base::BindRepeating(&GetEmptyVolumeInfo);
    options->cpu_fetcher = base::BindRepeating(&GetEmptyCPUStatistics);
    options->cpu_temp_fetcher = base::BindRepeating(&GetEmptyCPUTempInfo);
    options->android_status_fetcher =
        base::BindRepeating(&GetEmptyAndroidStatus);

    options->emmc_lifetime_fetcher =
        base::BindRepeating(&GetEmptyEMMCLifetimeEstimation);
    options->stateful_partition_info_fetcher =
        base::BindRepeating(&GetEmptyStatefulPartitionInfo);
    options->graphics_status_fetcher =
        base::BindRepeating(&GetEmptyGraphicsStatus);
    options->crash_report_info_fetcher =
        base::BindRepeating(&GetEmptyCrashReportInfo);
    options->app_info_generator =
        std::make_unique<AppInfoGenerator>(nullptr, base::Days(0));
    return options;
  }

  void GetStatus() {
    device_status_.Clear();
    session_status_.Clear();
    got_session_status_ = false;
    run_loop_ = std::make_unique<base::RunLoop>();
    status_collector_->GetStatusAsync(base::BindRepeating(
        &DeviceStatusCollectorTest::OnStatusReceived, base::Unretained(this)));
    run_loop_->Run();
    run_loop_.reset();
  }

  void OnStatusReceived(StatusCollectorParams callback_params) {
    if (callback_params.device_status) {
      device_status_ = *callback_params.device_status;
    }
    got_session_status_ = callback_params.session_status != nullptr;
    if (got_session_status_) {
      session_status_ = *callback_params.session_status;
    }
    EXPECT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void MockUserWithTypeAndAffiliation(const AccountId& account_id,
                                      user_manager::UserType user_type,
                                      bool is_affiliated) {
    // Build a profile with profile name=account e-mail because our testing
    // version of GetDMTokenForProfile returns the profile name.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    testing_profile_ = profile_builder.Build();

    auto* user_manager = GetFakeChromeUserManager();
    auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_type, testing_profile_.get());
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false, /*is_child=*/false);
  }

  void MockRegularUserWithAffiliation(const AccountId& account_id,
                                      bool is_affiliated) {
    MockUserWithTypeAndAffiliation(account_id, user_manager::UserType::kRegular,
                                   is_affiliated);
  }

  void MockRunningKioskApp(const DeviceLocalAccount& account,
                           const DeviceLocalAccountType& type) {
    user_manager::User* user = nullptr;
    auto* user_manager = GetFakeChromeUserManager();
    AccountId account_id = AccountId::FromUserEmail(account.user_id);
    switch (type) {
      case DeviceLocalAccountType::kKioskApp:
        user = user_manager->AddKioskAppUser(account_id);
        break;
      case DeviceLocalAccountType::kWebKioskApp:
        user = user_manager->AddWebKioskAppUser(account_id);
        break;
      default:
        FAIL() << "Unexpected kiosk app type.";
    }
    DCHECK(user);

    testing_profile_ = std::make_unique<TestingProfile>();
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());
    SetDeviceLocalAccountsForTesting(&owner_settings_service_, {account});
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
  }

  std::unique_ptr<ScopedChromeOSVersionInfo> MockPlatformVersion(
      const std::string& platform_version) {
    const std::string lsb_release = base::StringPrintf(
        "CHROMEOS_RELEASE_VERSION=%s", platform_version.c_str());
    return std::make_unique<ScopedChromeOSVersionInfo>(lsb_release,
                                                       base::Time::Now());
  }

  void MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      const DeviceLocalAccount& auto_launch_app_account,
      const std::string& required_platform_version) {
    if (!kiosk_chrome_app_manager_) {
      kiosk_chrome_app_manager_ =
          std::make_unique<ash::KioskChromeAppManager>();
    }
    kiosk_chrome_app_manager_->AddAppForTest(
        auto_launch_app_account.kiosk_app_id,
        AccountId::FromUserEmail(auto_launch_app_account.user_id),
        GURL("http://cws/"),  // Dummy URL to avoid setup ExtensionsClient.
        required_platform_version);

    std::vector<DeviceLocalAccount> accounts;
    accounts.push_back(auto_launch_app_account);
    SetDeviceLocalAccountsForTesting(&owner_settings_service_, accounts);

    owner_settings_service_.SetString(
        ash::kAccountsPrefDeviceLocalAccountAutoLoginId,
        auto_launch_app_account.account_id);

    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(
        required_platform_version,
        kiosk_chrome_app_manager_->GetAutoLaunchAppRequiredPlatformVersion());
  }

  void MockAutoLaunchWebKioskApp(
      const DeviceLocalAccount& auto_launch_app_account) {
    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();
    web_kiosk_app_manager_->AddAppForTesting(
        AccountId::FromUserEmail(auto_launch_app_account.user_id),
        GURL(auto_launch_app_account.web_kiosk_app_info.url()));

    std::vector<DeviceLocalAccount> accounts;
    accounts.push_back(auto_launch_app_account);
    SetDeviceLocalAccountsForTesting(&owner_settings_service_, accounts);

    owner_settings_service_.SetString(
        ash::kAccountsPrefDeviceLocalAccountAutoLoginId,
        auto_launch_app_account.account_id);

    base::RunLoop().RunUntilIdle();
  }

  void AddActivityPeriodForUser(const std::string& user_email) {
    base::Time start_time = test_clock_.Now();
    test_clock_.Advance(base::Hours(1));
    base::Time end_time = test_clock_.Now();
    test_clock_.Advance(base::Hours(1));

    status_collector_->GetActivityStorageForTesting().AddActivityPeriod(
        start_time, end_time, user_email);
  }

  // Convenience method.
  int64_t ActivePeriodMilliseconds() {
    return DeviceStatusCollector::kIdlePollInterval.InMilliseconds();
  }

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};

  ChromeContentClient content_client_;
  ChromeContentBrowserClient browser_content_client_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DiskMountManager::MountPoints mount_point_map_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::FakeOwnerSettingsService owner_settings_service_{
      scoped_testing_cros_settings_.device_settings(), nullptr};
  // Only set after MockRunningKioskApp was called.
  std::unique_ptr<TestingProfile> testing_profile_;
  // Only set after MockAutoLaunchWebKioskApp was called.
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;
  // Only set after MockAutoLaunchKioskAppWithRequiredPlatformVersion was
  // called.
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<ReportingUserTracker> reporting_user_tracker_;
  em::DeviceStatusReportRequest device_status_;
  em::SessionStatusReportRequest session_status_;
  bool got_session_status_;
  TestingPrefServiceSimple profile_pref_service_;
  std::unique_ptr<ManagedSessionService> managed_session_service_;
  std::unique_ptr<TestingDeviceStatusCollector> status_collector_;
  const DeviceLocalAccount fake_kiosk_device_local_account_;
  const WebKioskAppBasicInfo fake_web_kiosk_app_basic_info_;
  const DeviceLocalAccount fake_web_kiosk_device_local_account_;
  base::ScopedPathOverride user_data_dir_override_;
  base::ScopedPathOverride crash_dumps_dir_override_;
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged> update_engine_client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;

  apps::ScopedOmitBorealisAppsForTesting scoped_omit_borealis_apps_for_testing_;
  apps::ScopedOmitBuiltInAppsForTesting scoped_omit_built_in_apps_for_testing_;
  apps::ScopedOmitPluginVmAppsForTesting
      scoped_omit_plugin_vm_apps_for_testing_;

  ash::ScopedCrasAudioHandlerForTesting cras_audio_handler_;

  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
  std::unique_ptr<::ash::mojo_service_manager::FakeMojoServiceManager>
      fake_service_manager_;
};

TEST_F(DeviceStatusCollectorTest, AllIdle) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_IDLE, ui::IDLE_STATE_IDLE,
                                 ui::IDLE_STATE_IDLE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);

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
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);

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
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE,
                                 ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_IDLE,   ui::IDLE_STATE_IDLE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);

  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(4 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

// For kiosks report total uptime instead of only active periods.
TEST_F(DeviceStatusCollectorTest, MixedStatesForKiosk) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE, ui::IDLE_STATE_ACTIVE,
      ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE, ui::IDLE_STATE_IDLE,
  };
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE, ash::LoginState::LOGGED_IN_USER_KIOSK);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(6 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, StateKeptInPref) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE,
                                 ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_IDLE,   ui::IDLE_STATE_IDLE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector(CreateEmptyDeviceStatusCollectorOptions());
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));

  GetStatus();
  EXPECT_EQ(6 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityNotWrittenToProfilePref) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  EXPECT_THAT(profile_pref_service_.GetDict(prefs::kUserActivityTimes),
              IsEmpty());

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
  EXPECT_THAT(profile_pref_service_.GetDict(prefs::kUserActivityTimes),
              IsEmpty());
}

TEST_F(DeviceStatusCollectorTest, MaxStoredPeriods) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_IDLE};
  const int kMaxDays = 10;

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  status_collector_->set_max_stored_past_activity_interval(
      base::Days(kMaxDays - 1));
  status_collector_->set_max_stored_future_activity_interval(base::Days(1));
  test_clock_.SetNow(base::Time::Now().LocalMidnight());

  // Simulate 12 active periods.
  for (int i = 0; i < kMaxDays + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(ui::IdleState));
    // Advance the simulated clock by a day.
    test_clock_.Advance(base::Days(1));
  }

  // Check that we don't exceed the max number of periods.
  GetStatus();
  EXPECT_EQ(kMaxDays, device_status_.active_periods_size());

  // Simulate some future times.
  for (int i = 0; i < kMaxDays + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(ui::IdleState));
    // Advance the simulated clock by a day.
    test_clock_.Advance(base::Days(1));
  }
  // Set the clock back so the previous simulated times are in the future.
  test_clock_.Advance(-base::Days(20));

  // Collect one more data point to trigger pruning.
  status_collector_->Simulate(test_states, 1);

  // Check that we don't exceed the max number of periods.
  device_status_.clear_active_periods();
  GetStatus();
  EXPECT_LT(device_status_.active_periods_size(), kMaxDays);
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesEnabledByDefault) {
  // Device activity times should be reported by default.
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(),
            GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesOff) {
  // Device activity times should not be reported while disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, false);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(ui::IdleState));
  GetStatus();
  EXPECT_EQ(0, device_status_.active_periods_size());
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityCrossingMidnight) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);

  // Set the baseline time to 20 seconds before midnight.
  test_clock_.SetNow(base::Time::Now().LocalMidnight() - base::Seconds(20));

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
            base::Time::kMillisecondsPerDay);
  EXPECT_EQ(time_period1.end_timestamp() - time_period1.start_timestamp(),
            base::Time::kMillisecondsPerDay);
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesKeptUntilSubmittedSuccessfully) {
  DisableDefaultSettings();
  ui::IdleState test_states[] = {
      ui::IDLE_STATE_ACTIVE,
      ui::IDLE_STATE_ACTIVE,
  };
  // Make sure CPU stats get reported in time. If we don't run this, the second
  // call to |GetStatus()| will contain these stats, but the first call won't
  // and the EXPECT_EQ test below fails.
  base::RunLoop().RunUntilIdle();

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);

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
  // and prior activity is no longer showing.
  status_collector_->Simulate(test_states, 1);
  status_collector_->OnSubmittedSuccessfully();
  GetStatus();
  EXPECT_EQ(0, GetActiveMilliseconds(device_status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityNoUser) {
  DisableDefaultSettings();
  scoped_feature_list_.InitAndEnableFeature(
      features::kActivityReportingSessionType);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);

  EXPECT_FALSE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_FALSE(device_status_.active_periods(0).has_session_type());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithPublicSessionUser) {
  DisableDefaultSettings();
  scoped_feature_list_.InitAndEnableFeature(
      features::kActivityReportingSessionType);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);
  const AccountId public_account_id(AccountId::FromUserEmail(
      "public@public-accounts.device-local.localhost"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddPublicAccountUser(public_account_id);
  user_manager->UserLoggedIn(public_account_id, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  EXPECT_FALSE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_EQ(em::ActiveTimePeriod::SESSION_MANAGED_GUEST,
            device_status_.active_periods(0).session_type());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithKioskUser) {
  DisableDefaultSettings();
  scoped_feature_list_.InitAndEnableFeature(
      features::kActivityReportingSessionType);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);
  const AccountId public_account_id(
      AccountId::FromUserEmail("public@web-kiosk-apps.device-local.localhost"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddPublicAccountUser(public_account_id);
  user_manager->UserLoggedIn(public_account_id, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  EXPECT_FALSE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_EQ(em::ActiveTimePeriod::SESSION_WEB_KIOSK,
            device_status_.active_periods(0).session_type());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithAffiliatedUser) {
  DisableDefaultSettings();
  scoped_feature_list_.InitAndEnableFeature(
      features::kActivityReportingSessionType);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  EXPECT_TRUE(status_collector_->IsReportingActivityTimes());
  EXPECT_TRUE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_EQ(account_id0.GetUserEmail(),
            device_status_.active_periods(0).user_email());
  EXPECT_EQ(em::ActiveTimePeriod::SESSION_AFFILIATED_USER,
            device_status_.active_periods(0).session_type());
  device_status_.clear_active_periods();  // Clear the result protobuf.

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, false);

  EXPECT_TRUE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_FALSE(device_status_.active_periods(0).has_session_type());
}

TEST_F(DeviceStatusCollectorTest, ActivityWithNotAffiliatedUser) {
  DisableDefaultSettings();
  scoped_feature_list_.InitAndEnableFeature(
      features::kActivityReportingSessionType);
  ui::IdleState test_states[] = {ui::IDLE_STATE_ACTIVE, ui::IDLE_STATE_ACTIVE,
                                 ui::IDLE_STATE_ACTIVE};
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, false, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  EXPECT_FALSE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_FALSE(device_status_.active_periods(0).has_session_type());
  device_status_.clear_active_periods();  // Clear the result protobuf.

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, false);

  EXPECT_FALSE(status_collector_->IsReportingActivityTimes());
  EXPECT_FALSE(status_collector_->IsReportingUsers());

  status_collector_->Simulate(test_states, 3);
  GetStatus();
  EXPECT_EQ(1, device_status_.active_periods_size());
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_FALSE(device_status_.active_periods(0).has_session_type());
}

// Remove deprecated ARC Kiosk account from the activity periods list.
TEST_F(DeviceStatusCollectorTest, PrepopulatedActivityPeriods) {
  DisableDefaultSettings();

  // Simulate when something is left from the previous session reporting.
  // ARC Kiosk activity to be removed.
  const std::string kArcKioskEmail =
      std::string("test@") + user_manager::kArcKioskDomain;
  AddActivityPeriodForUser(kArcKioskEmail);
  // Regular user activity to be uploaded.
  const std::string kTestEmail = std::string("test@test.com");
  AddActivityPeriodForUser(kTestEmail);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceActivityTimes, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);

  GetStatus();
  EXPECT_EQ(2, device_status_.active_periods_size());
  // No email and session type are reported for ARC kiosk account.
  EXPECT_TRUE(device_status_.active_periods(0).user_email().empty());
  EXPECT_FALSE(device_status_.active_periods(0).has_session_type());
  // Both email and session type are reported for regular user account.
  EXPECT_EQ(device_status_.active_periods(1).user_email(), kTestEmail);
  EXPECT_TRUE(device_status_.active_periods(1).has_session_type());
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  // Test that boot mode data is reported by default.
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kDevSwitchBootKey, ash::system::kDevSwitchBootValueVerified);
  GetStatus();
  EXPECT_EQ("Verified", device_status_.boot_mode());

  // Test that boot mode data is not reported if the pref turned off.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceBootMode, false);

  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceBootMode, true);

  fake_statistics_provider_.SetMachineStatistic(ash::system::kDevSwitchBootKey,
                                                "(error)");
  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  fake_statistics_provider_.SetMachineStatistic(ash::system::kDevSwitchBootKey,
                                                " ");
  GetStatus();
  EXPECT_FALSE(device_status_.has_boot_mode());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kDevSwitchBootKey, ash::system::kDevSwitchBootValueVerified);
  GetStatus();
  EXPECT_EQ("Verified", device_status_.boot_mode());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kDevSwitchBootKey, ash::system::kDevSwitchBootValueDev);
  GetStatus();
  EXPECT_EQ("Dev", device_status_.boot_mode());
}

TEST_F(DeviceStatusCollectorTest, WriteProtectSwitch) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSystemInfo, true);
  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareWriteProtectCurrentKey,
      ash::system::kFirmwareWriteProtectCurrentValueOn);
  GetStatus();
  EXPECT_TRUE(device_status_.write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareWriteProtectCurrentKey, "(error)");
  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareWriteProtectCurrentKey, " ");
  GetStatus();
  EXPECT_FALSE(device_status_.has_write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareWriteProtectCurrentKey,
      ash::system::kFirmwareWriteProtectCurrentValueOn);
  GetStatus();
  EXPECT_TRUE(device_status_.write_protect_switch());

  fake_statistics_provider_.SetMachineStatistic(
      ash::system::kFirmwareWriteProtectCurrentKey,
      ash::system::kFirmwareWriteProtectCurrentValueOff);
  GetStatus();
  EXPECT_FALSE(device_status_.write_protect_switch());
}

TEST_F(DeviceStatusCollectorTest, VersionInfo) {
  // Expect the version info to be reported by default.
  GetStatus();
  EXPECT_TRUE(device_status_.has_browser_version());
  EXPECT_TRUE(device_status_.has_is_lacros_primary_browser());
  EXPECT_TRUE(device_status_.has_channel());
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_TRUE(device_status_.has_firmware_version());
  EXPECT_TRUE(device_status_.has_tpm_version_info());

  // Expect tpm version info is still set (with an empty one) regardless of
  // D-Bus error.
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_version_info_reply()
      ->set_status(::tpm_manager::STATUS_DBUS_ERROR);
  GetStatus();
  EXPECT_TRUE(device_status_.has_browser_version());
  EXPECT_TRUE(device_status_.has_is_lacros_primary_browser());
  EXPECT_TRUE(device_status_.has_channel());
  EXPECT_TRUE(device_status_.has_os_version());
  EXPECT_TRUE(device_status_.has_firmware_version());
  EXPECT_TRUE(device_status_.has_tpm_version_info());
  // Reset the version info reply just in case the rest of tests get affected.
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_version_info_reply()
      ->clear_status();

  // When the pref to collect this data is not enabled, expect that none of
  // the fields are present in the protobuf.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVersionInfo, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_browser_version());
  EXPECT_FALSE(device_status_.has_is_lacros_primary_browser());
  EXPECT_FALSE(device_status_.has_channel());
  EXPECT_FALSE(device_status_.has_os_version());
  EXPECT_FALSE(device_status_.has_firmware_version());
  EXPECT_FALSE(device_status_.has_tpm_version_info());

  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVersionInfo, true);
  GetStatus();
  EXPECT_TRUE(device_status_.has_browser_version());
  EXPECT_TRUE(device_status_.has_is_lacros_primary_browser());
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

  auto* user_manager = GetFakeChromeUserManager();
  auto* public_user = user_manager->AddPublicAccountUser(public_account_id);
  user_manager->UserLoggedIn(public_account_id, public_user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  auto* user0 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user0->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  auto* user1 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id1, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id1, user1->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  auto* user2 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id2, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id2, user2->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  auto* user3 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id3, false, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id3, user3->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  auto* user4 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id4, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id4, user4->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  auto* user5 = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id5, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id5, user5->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  // Verify that users are reported by default.
  GetStatus();
  EXPECT_EQ(6, device_status_.users_size());

  // Verify that users are reported after enabling the setting.
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceUsers, true);
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
      ash::kReportDeviceUsers, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.users_size());
}

TEST_F(DeviceStatusCollectorTest, TestVolumeInfo) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceStorageStatus, true);
  std::vector<std::string> expected_mount_points;
  std::vector<em::VolumeInfo> expected_volume_info;
  int size = 12345678;
  for (const auto& mount_info :
       DiskMountManager::GetInstance()->mount_points()) {
    expected_mount_points.push_back(mount_info.mount_path);
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
  EXPECT_THAT(expected_volume_info, Not(IsEmpty()));

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->volume_info_fetcher =
      base::BindRepeating(&GetFakeVolumeInfo, expected_volume_info);
  RestartStatusCollector(std::move(options));

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
}

TEST_F(DeviceStatusCollectorTest, TestSystemFreeRamInfo) {
  DisableDefaultSettings();
  const int sample_count =
      static_cast<const int>(DeviceStatusCollector::kMaxResourceUsageSamples);
  std::vector<int64_t> timestamp_lowerbounds;
  std::vector<int64_t> timestamp_upperbounds;
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceMemoryInfo, true);

  // Refresh our samples. Sample more than kMaxHardwareSamples times to
  // make sure that the code correctly caps the number of cached samples.
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < sample_count; ++i) {
    timestamp_lowerbounds.push_back(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
    timestamp_upperbounds.push_back(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
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

TEST_F(DeviceStatusCollectorTest, TestCPUInfos) {
  DisableDefaultSettings();
  // Mock 100% CPU usage.
  std::string full_cpu_usage("cpu  500 0 500 0 0 0 0");
  int64_t timestamp_lowerbound =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->cpu_fetcher =
      base::BindRepeating(&GetFakeCPUStatistics, full_cpu_usage);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCpuInfo, true);
  RestartStatusCollector(std::move(options));

  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  int64_t timestamp_upperbound =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  GetStatus();
  ASSERT_EQ(1, device_status_.cpu_utilization_infos().size());
  EXPECT_EQ(100, device_status_.cpu_utilization_infos(0).cpu_utilization_pct());
  EXPECT_GE(device_status_.cpu_utilization_infos(0).timestamp(),
            timestamp_lowerbound);
  EXPECT_LE(device_status_.cpu_utilization_infos(0).timestamp(),
            timestamp_upperbound);

  // Now sample CPU usage again (active usage counters will not increase
  // so should show 0% cpu usage).
  timestamp_lowerbound = base::Time::Now().InMillisecondsSinceUnixEpoch();
  status_collector_->RefreshSampleResourceUsage();
  base::RunLoop().RunUntilIdle();
  timestamp_upperbound = base::Time::Now().InMillisecondsSinceUnixEpoch();
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
    timestamp_lowerbounds.push_back(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    status_collector_->RefreshSampleResourceUsage();
    base::RunLoop().RunUntilIdle();
    timestamp_upperbounds.push_back(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
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
}

TEST_F(DeviceStatusCollectorTest, TestCPUTemp) {
  DisableDefaultSettings();
  std::vector<em::CPUTempInfo> expected_temp_info;
  int cpu_cnt = 12;
  int64_t timestamp_lowerbound =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  for (int i = 0; i < cpu_cnt; ++i) {
    em::CPUTempInfo info;
    info.set_cpu_temp(i * 10 + 100);
    info.set_cpu_label(base::StringPrintf("Core %d", i));
    info.set_timestamp(kFakeCpuTimestamp);
    expected_temp_info.push_back(info);
  }

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->cpu_temp_fetcher =
      base::BindRepeating(&GetFakeCPUTempInfo, expected_temp_info);
  RestartStatusCollector(std::move(options));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCpuInfo, true);

  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();

  GetStatus();
  int64_t timestamp_upperbound =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  EXPECT_EQ(expected_temp_info.size(),
            static_cast<size_t>(device_status_.cpu_temp_infos_size()));

  // Walk the returned CPUTempInfo to make sure it matches.
  for (const em::CPUTempInfo& expected_info : expected_temp_info) {
    bool found = false;
    for (const em::CPUTempInfo& info : device_status_.cpu_temp_infos()) {
      if (info.cpu_label() == expected_info.cpu_label()) {
        EXPECT_EQ(expected_info.cpu_temp(), info.cpu_temp());
        EXPECT_GE(info.timestamp(), timestamp_lowerbound);
        EXPECT_LE(info.timestamp(), timestamp_upperbound);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "No matching CPUTempInfo for "
                       << expected_info.cpu_label();
  }
}

TEST_F(DeviceStatusCollectorTest, TestDiskLifetimeEstimation) {
  DisableDefaultSettings();
  em::DiskLifetimeEstimation est;
  est.set_slc(10);
  est.set_mlc(15);
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->emmc_lifetime_fetcher =
      base::BindRepeating(&GetFakeEMMCLifetiemEstimation, est);
  RestartStatusCollector(std::move(options));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceStorageStatus, true);

  // Force finishing tasks posted by ctor of DeviceStatusCollector.
  content::RunAllTasksUntilIdle();
  GetStatus();
  EXPECT_TRUE(device_status_.storage_status().has_lifetime_estimation());
  EXPECT_TRUE(device_status_.storage_status().lifetime_estimation().has_slc());
  EXPECT_TRUE(device_status_.storage_status().lifetime_estimation().has_mlc());
  EXPECT_EQ(est.slc(),
            device_status_.storage_status().lifetime_estimation().slc());
  EXPECT_EQ(est.mlc(),
            device_status_.storage_status().lifetime_estimation().mlc());
}

TEST_F(DeviceStatusCollectorTest, KioskAndroidReporting) {
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);
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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

  // Mock Kiosk app, so some session status is reported
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));
  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  // Note that this relies on the fact that kReportArcStatusEnabled is false by
  // default.
  EXPECT_FALSE(session_status_.has_android_status());
}

TEST_F(DeviceStatusCollectorTest, RegularUserAndroidReporting) {
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  scoped_feature_list_.InitWithFeatures(
      {features::kCrostiniAdditionalEnterpriseReporting}, {});

  const std::string desktop_file_id = "vim";
  const std::string package_id =
      "vim;2:8.0.0197-4+deb9u1;amd64;installed:debian-stable";

  vm_tools::apps::ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList(
          desktop_file_id, crostini::kCrostiniDefaultVmName,
          crostini::kCrostiniDefaultContainerName);
  app_list.mutable_apps(0)->set_package_id(package_id);

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(
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

  EXPECT_EQ(1, session_status_.crostini_status().installed_apps_size());
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

  // In tests, GetUserDMToken returns the e-mail for easy verification.
  EXPECT_EQ(account_id.GetUserEmail(), session_status_.user_dm_token());
}

TEST_F(DeviceStatusCollectorTest,
       TerminalAppIsNotReportedIfCrostiniHasBeenRemoved) {
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->android_status_fetcher =
      base::BindRepeating(&GetFakeAndroidStatus, kArcStatus, kDroidGuardInfo);
  RestartStatusCollector(std::move(options));

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
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSecurityStatus, true);
  auto* tpm_status_reply = chromeos::TpmManagerClient::Get()
                               ->GetTestInterface()
                               ->mutable_nonsensitive_status_reply();
  tpm_status_reply->set_is_enabled(true);
  tpm_status_reply->set_is_owned(true);
  tpm_status_reply->set_is_owner_password_present(false);
  auto* enrollment_status_reply =
      ash::AttestationClient::Get()->GetTestInterface()->mutable_status_reply();
  enrollment_status_reply->set_prepared_for_enrollment(true);
  enrollment_status_reply->set_enrolled(false);
  auto* da_info_reply = chromeos::TpmManagerClient::Get()
                            ->GetTestInterface()
                            ->mutable_dictionary_attack_info_reply();
  da_info_reply->set_dictionary_attack_counter(5);
  da_info_reply->set_dictionary_attack_threshold(10);
  da_info_reply->set_dictionary_attack_lockout_in_effect(false);
  da_info_reply->set_dictionary_attack_lockout_seconds_remaining(0);
  auto* supported_features_reply = chromeos::TpmManagerClient::Get()
                                       ->GetTestInterface()
                                       ->mutable_supported_features_reply();
  supported_features_reply->set_is_allowed(false);
  supported_features_reply->set_support_pinweaver(false);
  supported_features_reply->set_support_runtime_selection(false);
  supported_features_reply->set_support_u2f(false);

  GetStatus();

  EXPECT_TRUE(device_status_.has_tpm_status_info());
  EXPECT_EQ(tpm_status_reply->is_enabled(),
            device_status_.tpm_status_info().enabled());
  EXPECT_EQ(tpm_status_reply->is_owned(),
            device_status_.tpm_status_info().owned());
  EXPECT_EQ(tpm_status_reply->is_owned() &&
                !tpm_status_reply->is_owner_password_present(),
            device_status_.tpm_status_info().tpm_initialized());
  EXPECT_EQ(enrollment_status_reply->prepared_for_enrollment(),
            device_status_.tpm_status_info().attestation_prepared());
  EXPECT_EQ(enrollment_status_reply->enrolled(),
            device_status_.tpm_status_info().attestation_enrolled());
  EXPECT_EQ(static_cast<int32_t>(da_info_reply->dictionary_attack_counter()),
            device_status_.tpm_status_info().dictionary_attack_counter());
  EXPECT_EQ(static_cast<int32_t>(da_info_reply->dictionary_attack_threshold()),
            device_status_.tpm_status_info().dictionary_attack_threshold());
  EXPECT_EQ(
      da_info_reply->dictionary_attack_lockout_in_effect(),
      device_status_.tpm_status_info().dictionary_attack_lockout_in_effect());
  EXPECT_EQ(static_cast<int32_t>(
                da_info_reply->dictionary_attack_lockout_seconds_remaining()),
            device_status_.tpm_status_info()
                .dictionary_attack_lockout_seconds_remaining());
  EXPECT_EQ(false, device_status_.tpm_status_info().boot_lockbox_finalized());
  EXPECT_EQ(tpm_status_reply->is_owner_password_present(),
            device_status_.tpm_status_info().owner_password_is_present());
  EXPECT_EQ(
      supported_features_reply->is_allowed(),
      device_status_.tpm_status_info().tpm_supported_features().is_allowed());
  EXPECT_EQ(supported_features_reply->support_pinweaver(),
            device_status_.tpm_status_info()
                .tpm_supported_features()
                .support_pinweaver());
  EXPECT_EQ(supported_features_reply->support_runtime_selection(),
            device_status_.tpm_status_info()
                .tpm_supported_features()
                .support_runtime_selection());
  EXPECT_EQ(
      supported_features_reply->support_u2f(),
      device_status_.tpm_status_info().tpm_supported_features().support_u2f());
}

// Checks if tpm status is partially reported even if any error happens
// among the multiple D-Bus calls.
TEST_F(DeviceStatusCollectorTest, TpmStatusReportingAnyDBusError) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSecurityStatus, true);
  auto* tpm_status_reply = chromeos::TpmManagerClient::Get()
                               ->GetTestInterface()
                               ->mutable_nonsensitive_status_reply();
  auto* enrollment_status_reply =
      ash::AttestationClient::Get()->GetTestInterface()->mutable_status_reply();
  auto* da_info_reply = chromeos::TpmManagerClient::Get()
                            ->GetTestInterface()
                            ->mutable_dictionary_attack_info_reply();
  auto* supported_features_reply = chromeos::TpmManagerClient::Get()
                                       ->GetTestInterface()
                                       ->mutable_supported_features_reply();

  tpm_status_reply->set_status(::tpm_manager::STATUS_DBUS_ERROR);
  enrollment_status_reply->set_prepared_for_enrollment(true);
  GetStatus();
  EXPECT_EQ(enrollment_status_reply->prepared_for_enrollment(),
            device_status_.tpm_status_info().attestation_prepared());
  EXPECT_TRUE(device_status_.tpm_status_info().has_tpm_supported_features());
  // Reset the error status.
  tpm_status_reply->set_status(::tpm_manager::STATUS_SUCCESS);

  RestartStatusCollector();

  enrollment_status_reply->set_status(::attestation::STATUS_DBUS_ERROR);
  da_info_reply->set_dictionary_attack_counter(5);
  GetStatus();
  // Reset the error status.
  EXPECT_EQ(static_cast<int32_t>(da_info_reply->dictionary_attack_counter()),
            device_status_.tpm_status_info().dictionary_attack_counter());
  // Reset the error status.
  enrollment_status_reply->set_status(::attestation::STATUS_SUCCESS);

  RestartStatusCollector();

  da_info_reply->set_status(::tpm_manager::STATUS_DBUS_ERROR);
  tpm_status_reply->set_is_enabled(true);
  GetStatus();
  EXPECT_TRUE(device_status_.has_tpm_status_info());
  EXPECT_EQ(tpm_status_reply->is_enabled(),
            device_status_.tpm_status_info().enabled());
  // Reset the error status.
  da_info_reply->set_status(::tpm_manager::STATUS_SUCCESS);

  RestartStatusCollector();

  supported_features_reply->set_status(::tpm_manager::STATUS_DBUS_ERROR);
  tpm_status_reply->set_is_enabled(true);
  GetStatus();
  EXPECT_TRUE(device_status_.has_tpm_status_info());
  EXPECT_EQ(tpm_status_reply->is_enabled(),
            device_status_.tpm_status_info().enabled());
  EXPECT_FALSE(device_status_.tpm_status_info().has_tpm_supported_features());
  // Reset the error status (for symmetry).
  supported_features_reply->set_status(::tpm_manager::STATUS_SUCCESS);
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
      ash::kReportDeviceSessionStatus, true);
  GetStatus();
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, NoSessionStatusIfSessionReportingDisabled) {
  // Should not report session status if session status reporting is disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSessionStatus, false);
  // ReportDeviceSessionStatus only controls Kiosk reporting, ARC reporting
  // has to be disabled serarately.
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));
  // Set up a device-local account for single-app kiosk mode.
  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);
  testing_profile_->GetPrefs()->SetBoolean(prefs::kReportArcStatusEnabled,
                                           false);

  GetStatus();
  EXPECT_FALSE(got_session_status_);
}

TEST_F(DeviceStatusCollectorTest, ReportKioskSessionStatus) {
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSessionStatus, true);
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));

  // Set up a device-local account for single-app kiosk mode.
  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);

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

TEST_F(DeviceStatusCollectorTest, ReportWebKioskSessionStatus) {
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSessionStatus, true);
  status_collector_->set_kiosk_account(std::make_unique<DeviceLocalAccount>(
      fake_web_kiosk_device_local_account_));

  // Set up a device-local account for single-app Web kiosk mode.
  MockRunningKioskApp(fake_web_kiosk_device_local_account_,
                      DeviceLocalAccountType::kWebKioskApp);

  GetStatus();
  EXPECT_TRUE(got_session_status_);
  ASSERT_EQ(1, session_status_.installed_apps_size());
  EXPECT_EQ(kWebKioskAccountId, session_status_.device_local_account_id());
  const em::AppStatus app = session_status_.installed_apps(0);
  EXPECT_EQ(kWebKioskAppUrl, app.app_id());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
  // Expect no User DM Token for kiosk sessions.
  EXPECT_FALSE(session_status_.has_user_dm_token());
}

TEST_F(DeviceStatusCollectorTest, NoOsUpdateStatusByDefault) {
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.has_os_update_status());
}

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatusUpToDate) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);

  const char* kRequiredPlatformVersions[] = {"1234", "1234.0", "1234.0.0"};

  for (size_t i = 0; i < std::size(kRequiredPlatformVersions); ++i) {
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

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatusUpToDate_NonKiosk) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);
  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_UP_TO_DATE,
            device_status_.os_update_status().update_status());
  ASSERT_TRUE(device_status_.os_update_status().has_last_checked_timestamp());
  ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());
  ASSERT_FALSE(device_status_.os_update_status().has_new_platform_version());
}

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatus) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);
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

  for (size_t i = 0; i < std::size(kUpdateEngineOps); ++i) {
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

TEST_F(DeviceStatusCollectorTest, ReportOsUpdateStatus_NonKiosk) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);

  update_engine::StatusResult update_status;
  update_status.set_current_operation(update_engine::Operation::IDLE);

  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_UP_TO_DATE,
            device_status_.os_update_status().update_status());
  ASSERT_TRUE(device_status_.os_update_status().has_last_checked_timestamp());
  ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());
  ASSERT_FALSE(
      device_status_.os_update_status().has_new_required_platform_version());

  const update_engine::Operation kUpdateEngineOps[] = {
      update_engine::Operation::DOWNLOADING,
      update_engine::Operation::VERIFYING,
      update_engine::Operation::FINALIZING,
  };

  for (size_t i = 0; i < std::size(kUpdateEngineOps); ++i) {
    update_status.set_current_operation(kUpdateEngineOps[i]);
    update_status.set_new_version("1235.1.2");
    update_engine_client_->PushLastStatus(update_status);

    GetStatus();
    ASSERT_TRUE(device_status_.has_os_update_status());
    EXPECT_EQ(em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS,
              device_status_.os_update_status().update_status());
    EXPECT_EQ("1235.1.2",
              device_status_.os_update_status().new_platform_version());
    ASSERT_TRUE(device_status_.os_update_status().has_last_checked_timestamp());
    ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());
    ASSERT_FALSE(
        device_status_.os_update_status().has_new_required_platform_version());
  }

  update_status.set_current_operation(
      update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client_->PushLastStatus(update_status);
  GetStatus();
  ASSERT_TRUE(device_status_.has_os_update_status());
  EXPECT_EQ(em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT,
            device_status_.os_update_status().update_status());
  ASSERT_TRUE(device_status_.os_update_status().has_last_checked_timestamp());
  ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());
  ASSERT_FALSE(
      device_status_.os_update_status().has_new_required_platform_version());
}

TEST_F(DeviceStatusCollectorTest, NoLastCheckedTimestampByDefault) {
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.os_update_status().has_last_checked_timestamp());
}

TEST_F(DeviceStatusCollectorTest, ReportLastCheckedTimestamp) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);

  // Check update multiple times, the timestamp stored in device status should
  // change accordingly.
  const int64 kLastCheckedTimes[] = {10, 20, 30};

  for (size_t i = 0; i < std::size(kLastCheckedTimes); ++i) {
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
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.os_update_status().has_last_reboot_timestamp());
}

TEST_F(DeviceStatusCollectorTest, ReportLastRebootTimestamp) {
  DisableDefaultSettings();
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportOsUpdateStatus, true);

  GetStatus();
  ASSERT_TRUE(device_status_.os_update_status().has_last_reboot_timestamp());

  // No good way to inject specific last reboot timestamp of the test machine,
  // so just make sure UnixEpoch < RebootTime < Now.
  EXPECT_GT(device_status_.os_update_status().last_reboot_timestamp(),
            base::Time::UnixEpoch().InMillisecondsSinceUnixEpoch());
  EXPECT_LT(device_status_.os_update_status().last_reboot_timestamp(),
            base::Time::Now().InMillisecondsSinceUnixEpoch());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppByDefault) {
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));

  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);
  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, NoRunningKioskAppWhenNotInKioskSession) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportRunningKioskApp, true);
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, kDefaultPlatformVersion);

  GetStatus();
  EXPECT_FALSE(device_status_.has_running_kiosk_app());
}

TEST_F(DeviceStatusCollectorTest, ReportRunningKioskApp) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportRunningKioskApp, true);
  auto scoped_version = MockPlatformVersion(kDefaultPlatformVersion);
  MockAutoLaunchKioskAppWithRequiredPlatformVersion(
      fake_kiosk_device_local_account_, "1235");

  MockRunningKioskApp(fake_kiosk_device_local_account_,
                      DeviceLocalAccountType::kKioskApp);
  status_collector_->set_kiosk_account(
      std::make_unique<DeviceLocalAccount>(fake_kiosk_device_local_account_));

  GetStatus();
  ASSERT_TRUE(device_status_.has_running_kiosk_app());
  const em::AppStatus app = device_status_.running_kiosk_app();
  EXPECT_EQ(kKioskAppId, app.app_id());
  EXPECT_EQ("1235", app.required_platform_version());
  EXPECT_FALSE(app.has_status());
  EXPECT_FALSE(app.has_error());
}

TEST_F(DeviceStatusCollectorTest, ReportRunningWebKioskApp) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportRunningKioskApp, true);
  MockAutoLaunchWebKioskApp(fake_web_kiosk_device_local_account_);

  MockRunningKioskApp(fake_web_kiosk_device_local_account_,
                      DeviceLocalAccountType::kWebKioskApp);
  status_collector_->set_kiosk_account(std::make_unique<DeviceLocalAccount>(
      fake_web_kiosk_device_local_account_));

  GetStatus();
  ASSERT_TRUE(device_status_.has_running_kiosk_app());
  const em::AppStatus app = device_status_.running_kiosk_app();
  EXPECT_EQ(kWebKioskAppUrl, app.app_id());
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
      ash::kReportDeviceAudioStatus, false);
  GetStatus();
  EXPECT_FALSE(device_status_.has_sound_volume());

  // Try setting a custom volume value and check that it matches.
  DisableDefaultSettings();
  const int kCustomVolume = 42;
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceAudioStatus, true);
  ash::CrasAudioHandler::Get()->SetOutputVolumePercent(kCustomVolume);
  GetStatus();
  EXPECT_EQ(kCustomVolume, device_status_.sound_volume());
}

TEST_F(DeviceStatusCollectorTest, TestStatefulPartitionInfo) {
  DisableDefaultSettings();
  // Create a fake stateful partition info and populate it with some arbitrary
  // values.
  em::StatefulPartitionInfo fakeStatefulPartitionInfo;
  fakeStatefulPartitionInfo.set_available_space(350);
  fakeStatefulPartitionInfo.set_total_space(500);
  fakeStatefulPartitionInfo.set_mount_source("mount_source");
  fakeStatefulPartitionInfo.set_filesystem(kFilesystem);

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->stateful_partition_info_fetcher = base::BindRepeating(
      &GetFakeStatefulPartitionInfo, fakeStatefulPartitionInfo);
  RestartStatusCollector(std::move(options));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceStorageStatus, true);

  GetStatus();

  EXPECT_TRUE(device_status_.has_stateful_partition_info());
  EXPECT_EQ(fakeStatefulPartitionInfo.available_space(),
            device_status_.stateful_partition_info().available_space());
  EXPECT_EQ(fakeStatefulPartitionInfo.total_space(),
            device_status_.stateful_partition_info().total_space());
  EXPECT_EQ(fakeStatefulPartitionInfo.mount_source(),
            device_status_.stateful_partition_info().mount_source());
  EXPECT_EQ(fakeStatefulPartitionInfo.filesystem(),
            device_status_.stateful_partition_info().filesystem());
}

TEST_F(DeviceStatusCollectorTest, TestRootDeviceStorage) {
  DisableDefaultSettings();
  static constexpr int64_t kRootDeviceRoundedSize = 128LL * 1024 * 1024 * 1024;
  static constexpr int64_t kRootDeviceSize = kRootDeviceRoundedSize - 85;
  ash::FakeSpacedClient::Get()->set_root_device_size(kRootDeviceSize);

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  RestartStatusCollector(std::move(options));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceStorageStatus, true);

  GetStatus();

  ASSERT_TRUE(device_status_.has_root_device_total_storage_bytes());
  EXPECT_EQ(device_status_.root_device_total_storage_bytes(),
            kRootDeviceRoundedSize);
}

TEST_F(DeviceStatusCollectorTest, TestGraphicsStatus) {
  DisableDefaultSettings();
  // Create a fake graphics status and populate it with some arbitrary values.
  em::GraphicsStatus fakeGraphicsStatus;

  // Create a fake display and populate it with some arbitrary values.
  uint64 num_displays = 0;
  for (uint64 i = 0; i < num_displays; i++) {
    em::DisplayInfo* display_info = fakeGraphicsStatus.add_displays();
    display_info->set_resolution_width(1920 * i);
    display_info->set_resolution_height(1080 * i);
    display_info->set_refresh_rate(60.0f * i);
    display_info->set_is_internal(i == 1);
  }

  em::GraphicsAdapterInfo* graphics_info = fakeGraphicsStatus.mutable_adapter();
  graphics_info->set_name("fake_adapter_name");
  graphics_info->set_driver_version("fake_driver_version");
  graphics_info->set_device_id(12345);
  graphics_info->set_system_ram_usage(15 * 1024 * 1024);

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->graphics_status_fetcher =
      base::BindRepeating(&GetFakeGraphicsStatus, fakeGraphicsStatus);
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceGraphicsStatus, true);

  GetStatus();

  EXPECT_TRUE(device_status_.has_graphics_status());

  for (uint64 i = 0; i < num_displays; i++) {
    EXPECT_EQ(fakeGraphicsStatus.displays(i).resolution_width(),
              device_status_.graphics_status().displays(i).resolution_width());
    EXPECT_EQ(fakeGraphicsStatus.displays(i).resolution_height(),
              device_status_.graphics_status().displays(i).resolution_height());
    EXPECT_EQ(fakeGraphicsStatus.displays(i).refresh_rate(),
              device_status_.graphics_status().displays(i).refresh_rate());
    EXPECT_EQ(fakeGraphicsStatus.displays(i).is_internal(),
              device_status_.graphics_status().displays(i).is_internal());
  }

  EXPECT_EQ(fakeGraphicsStatus.adapter().name(),
            device_status_.graphics_status().adapter().name());
  EXPECT_EQ(fakeGraphicsStatus.adapter().driver_version(),
            device_status_.graphics_status().adapter().driver_version());
  EXPECT_EQ(fakeGraphicsStatus.adapter().device_id(),
            device_status_.graphics_status().adapter().device_id());
  EXPECT_EQ(fakeGraphicsStatus.adapter().system_ram_usage(),
            device_status_.graphics_status().adapter().system_ram_usage());

  // Change the policy to not report display and graphics statuses
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceGraphicsStatus, false);

  GetStatus();

  EXPECT_FALSE(device_status_.has_graphics_status());
}

TEST_F(DeviceStatusCollectorTest, TestCrashReportInfo) {
  DisableDefaultSettings();
  // Create sample crash reports.
  std::vector<em::CrashReportInfo> expected_crash_report_infos;
  const base::Time now = base::Time::Now();
  const int report_cnt = 5;

  for (int i = 0; i < report_cnt; ++i) {
    base::Time timestamp = now - base::Hours(30) * i;

    em::CrashReportInfo info;
    info.set_capture_timestamp(timestamp.InMillisecondsSinceUnixEpoch());
    info.set_remote_id(base::StringPrintf("remote_id %d", i));
    info.set_cause(base::StringPrintf("cause %d", i));
    info.set_upload_status(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED);
    expected_crash_report_infos.push_back(info);
  }

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      base::BindRepeating(&GetFakeCrashReportInfo, expected_crash_report_infos);
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(report_cnt, device_status_.crash_report_infos_size());

  // Walk the returned CrashReportInfo to make sure it matches.
  for (const em::CrashReportInfo& expected_info : expected_crash_report_infos) {
    bool found = false;
    for (const em::CrashReportInfo& info :
         device_status_.crash_report_infos()) {
      if (info.remote_id() == expected_info.remote_id()) {
        EXPECT_EQ(expected_info.capture_timestamp(), info.capture_timestamp());
        EXPECT_EQ(expected_info.remote_id(), info.remote_id());
        EXPECT_EQ(expected_info.cause(), info.cause());
        EXPECT_EQ(expected_info.upload_status(), info.upload_status());
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "No matching CrashReportInfo for "
                       << expected_info.remote_id();
  }

  // Get the status again to make sure that the data keeps consistent.
  GetStatus();
  EXPECT_EQ(report_cnt, device_status_.crash_report_infos_size());
}

TEST_F(DeviceStatusCollectorTest,
       TestCrashReportInfo_TurnOffReportDeviceCrashReportInfo) {
  DisableDefaultSettings();
  // Create sample crash reports.
  std::vector<em::CrashReportInfo> expected_crash_report_infos;
  const base::Time now = base::Time::Now();
  const int report_cnt = 5;

  for (int i = 0; i < report_cnt; ++i) {
    base::Time timestamp = now - base::Hours(30) * i;

    em::CrashReportInfo info;
    info.set_capture_timestamp(timestamp.InMillisecondsSinceUnixEpoch());
    info.set_remote_id(base::StringPrintf("remote_id %d", i));
    info.set_cause(base::StringPrintf("cause %d", i));
    info.set_upload_status(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED);
    expected_crash_report_infos.push_back(info);
  }

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      base::BindRepeating(&GetFakeCrashReportInfo, expected_crash_report_infos);
  RestartStatusCollector(std::move(options));

  // Turn off kReportDeviceCrashReportInfo, but turn on kStatsReportingPref.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, false);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(0, device_status_.crash_report_infos_size());
}

TEST_F(DeviceStatusCollectorTest,
       TestCrashReportInfo_TurnOffStatsReportingPref) {
  DisableDefaultSettings();
  // Create sample crash reports.
  std::vector<em::CrashReportInfo> expected_crash_report_infos;
  const base::Time now = base::Time::Now();
  const int report_cnt = 5;

  for (int i = 0; i < report_cnt; ++i) {
    base::Time timestamp = now - base::Hours(30) * i;

    em::CrashReportInfo info;
    info.set_capture_timestamp(timestamp.InMillisecondsSinceUnixEpoch());
    info.set_remote_id(base::StringPrintf("remote_id %d", i));
    info.set_cause(base::StringPrintf("cause %d", i));
    info.set_upload_status(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED);
    expected_crash_report_infos.push_back(info);
  }

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      base::BindRepeating(&GetFakeCrashReportInfo, expected_crash_report_infos);
  RestartStatusCollector(std::move(options));

  // Turn on kReportDeviceCrashReportInfo, but turn off kStatsReportingPref.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, false);

  GetStatus();
  EXPECT_EQ(0, device_status_.crash_report_infos_size());
}

TEST_F(DeviceStatusCollectorTest, TestCrashReportInfo_DeviceRestartOnly) {
  DisableDefaultSettings();
  // Create a test uploads.log file with three kinds of source. The first two
  // lead to device restart, the third doesn't.
  std::vector<std::string> causes = {kTestCauseKernel, kTestCauseEC,
                                     kTestCauseOther};
  base::Time timestamp = base::Time::Now() - base::Hours(1);
  std::stringstream stream;
  for (int i = 0; i <= 2; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << timestamp.ToTimeT() << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << timestamp.ToTimeT() << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded) << ",";
    stream << "\"source\":\"" << causes[i] << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      DeviceStatusCollector::CrashReportInfoFetcher();
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(2, device_status_.crash_report_infos_size());

  // Walk the returned CrashReportInfo to make sure it matches.
  const em::CrashReportInfo& info0 = device_status_.crash_report_infos(0);
  EXPECT_EQ(timestamp.ToTimeT() * 1000, info0.capture_timestamp());
  EXPECT_EQ(kTestUploadId, info0.remote_id());
  EXPECT_EQ(kTestCauseEC, info0.cause());
  EXPECT_EQ(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED, info0.upload_status());

  const em::CrashReportInfo& info1 = device_status_.crash_report_infos(1);
  EXPECT_EQ(timestamp.ToTimeT() * 1000, info1.capture_timestamp());
  EXPECT_EQ(kTestUploadId, info1.remote_id());
  EXPECT_EQ(kTestCauseKernel, info1.cause());
  EXPECT_EQ(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED, info1.upload_status());
}

TEST_F(DeviceStatusCollectorTest, TestCrashReportInfo_LastDayUploadedOnly) {
  DisableDefaultSettings();
  // Create a test uploads.log file. One |upload_time| is within last 24 hours,
  // the other is not.
  base::Time now = base::Time::Now();
  base::Time timestamps[] = {now - base::Hours(22), now - base::Hours(24)};

  std::stringstream stream;
  for (int i = 0; i <= 1; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << timestamps[i].ToTimeT() << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << timestamps[i].ToTimeT() << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded) << ",";
    stream << "\"source\":\"" << kTestCauseKernel << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      DeviceStatusCollector::CrashReportInfoFetcher();
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(1, device_status_.crash_report_infos_size());

  // Walk the returned CrashReportInfo to make sure it matches.
  const em::CrashReportInfo& info = device_status_.crash_report_infos(0);
  EXPECT_EQ(timestamps[0].ToTimeT() * 1000, info.capture_timestamp());
  EXPECT_EQ(kTestUploadId, info.remote_id());
  EXPECT_EQ(kTestCauseKernel, info.cause());
  EXPECT_EQ(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED, info.upload_status());
}

TEST_F(DeviceStatusCollectorTest, TestCrashReportInfo_CrashReportEntryMaxSize) {
  DisableDefaultSettings();
  // Create a test uploads.log file with 200 entries. Only the last 100 is
  // included.
  base::Time timestamp = base::Time::Now() - base::Hours(1);
  const int report_cnt = 200;
  std::stringstream stream;
  for (int i = 1; i <= report_cnt; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << timestamp.ToTimeT() << "\",";
    stream << "\"upload_id\":\"" << i << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << timestamp.ToTimeT() << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded) << ",";
    stream << "\"source\":\"" << kTestCauseKernel << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      DeviceStatusCollector::CrashReportInfoFetcher();
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(100, device_status_.crash_report_infos_size());

  // Walk the returned CrashReportInfo to make sure it matches.
  for (int i = 0; i < 100; i++) {
    const em::CrashReportInfo& info = device_status_.crash_report_infos(i);
    EXPECT_EQ(timestamp.ToTimeT() * 1000, info.capture_timestamp());
    EXPECT_EQ(base::NumberToString(report_cnt - i), info.remote_id());
    EXPECT_EQ(kTestCauseKernel, info.cause());
    EXPECT_EQ(em::CrashReportInfo::UPLOAD_STATUS_UPLOADED,
              info.upload_status());
  }
}

TEST_F(DeviceStatusCollectorTest, TestCrashReportInfo_LegacyCSV) {
  DisableDefaultSettings();
  // Create a test uploads.log file in the legacy CSV format. All such kind of
  // record will be ignored because the required source filed is not existing.
  base::Time timestamp = base::Time::Now() - base::Hours(1);
  std::string test_entry =
      base::StringPrintf("%" PRId64, static_cast<int64_t>(timestamp.ToTimeT()));
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  WriteUploadLog(test_entry);

  auto options = CreateEmptyDeviceStatusCollectorOptions();
  options->crash_report_info_fetcher =
      DeviceStatusCollector::CrashReportInfoFetcher();
  RestartStatusCollector(std::move(options));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCrashReportInfo, true);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kStatsReportingPref, true);

  GetStatus();
  EXPECT_EQ(0, device_status_.crash_report_infos_size());
}

TEST_F(DeviceStatusCollectorTest, TestHealthdBacklightInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_EQ(device_status_.backlight_info_size(), 0);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceBacklightInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the backlight info.
  ASSERT_EQ(device_status_.backlight_info_size(), 1);
  const auto& backlight = device_status_.backlight_info(0);
  EXPECT_EQ(backlight.path(), kFakeBacklightPath);
  EXPECT_EQ(backlight.max_brightness(), kFakeMaxBrightness);
  EXPECT_EQ(backlight.brightness(), kFakeBrightness);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdPowerStatus) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_power_status());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDevicePowerStatus, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the battery data.
  ASSERT_TRUE(device_status_.has_power_status());
  ASSERT_EQ(device_status_.power_status().batteries_size(), 1);
  const auto& battery = device_status_.power_status().batteries(0);
  EXPECT_EQ(battery.serial(), kFakeBatterySerial);
  EXPECT_EQ(battery.manufacturer(), kFakeBatteryVendor);
  EXPECT_EQ(battery.design_capacity(), kExpectedBatteryChargeFullDesign);
  EXPECT_EQ(battery.full_charge_capacity(), kExpectedBatteryChargeFull);
  EXPECT_EQ(battery.cycle_count(), kFakeBatteryCycleCount);
  EXPECT_EQ(battery.design_min_voltage(), kExpectedBatteryVoltageMinDesign);
  EXPECT_EQ(battery.manufacture_date(), kFakeSmartBatteryManufactureDate);
  EXPECT_EQ(battery.technology(), kFakeBatteryTechnology);

  // Verify the battery sample data.
  ASSERT_EQ(battery.samples_size(), 1);
  const auto& battery_sample = battery.samples(0);
  EXPECT_EQ(battery_sample.voltage(), kExpectedBatteryVoltageNow);
  EXPECT_EQ(battery_sample.remaining_capacity(), kExpectedBatteryChargeNow);
  EXPECT_EQ(battery_sample.temperature(),
            (kFakeSmartBatteryTemperature - kZeroCInDeciKelvin) / 10);
  EXPECT_EQ(battery_sample.current(), kExpectedBatteryCurrentNow);
  EXPECT_EQ(battery_sample.status(), kFakeBatteryStatus);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdMemoryInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_memory_info());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceMemoryInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the memory info.
  ASSERT_TRUE(device_status_.has_memory_info());
  EXPECT_EQ(device_status_.memory_info().total_memory_kib(), kFakeTotalMemory);
  EXPECT_EQ(device_status_.memory_info().free_memory_kib(), kFakeFreeMemory);
  EXPECT_EQ(device_status_.memory_info().available_memory_kib(),
            kFakeAvailableMemory);
  EXPECT_EQ(device_status_.memory_info().page_faults_since_last_boot(),
            kFakePageFaults);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdCpuInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_global_cpu_info());
  EXPECT_EQ(device_status_.cpu_info_size(), 0);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCpuInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the CPU data.
  ASSERT_TRUE(device_status_.has_global_cpu_info());
  EXPECT_EQ(device_status_.global_cpu_info().num_total_threads(),
            kFakeNumTotalThreads);

  // Verify the physical CPU.
  ASSERT_EQ(device_status_.cpu_info_size(), 1);
  const auto& cpu = device_status_.cpu_info(0);
  EXPECT_EQ(cpu.model_name(), kFakeModelName);
  EXPECT_EQ(cpu.architecture(), kFakeProtoArchitecture);
  EXPECT_EQ(cpu.max_clock_speed_khz(), kFakeMaxClockSpeed);
  // Verify the logical CPU.
  ASSERT_EQ(cpu.logical_cpus_size(), 1);
  const auto& logical_cpu = cpu.logical_cpus(0);
  EXPECT_EQ(logical_cpu.scaling_max_frequency_khz(), kFakeScalingMaxFrequency);
  EXPECT_EQ(logical_cpu.scaling_current_frequency_khz(),
            kFakeScalingCurFrequency);
  EXPECT_EQ(logical_cpu.idle_time_seconds(), kFakeIdleTime);
  // Verify the C-state data.
  ASSERT_EQ(logical_cpu.c_states_size(), 1);
  const auto& c_state = logical_cpu.c_states(0);
  EXPECT_EQ(c_state.name(), kFakeCStateName);
  EXPECT_EQ(c_state.time_in_state_since_last_boot_us(),
            kFakeTimeInStateSinceLastBoot);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdStorageStatus) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_storage_status());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceStorageStatus, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the storage data.
  ASSERT_TRUE(device_status_.has_storage_status());
  ASSERT_EQ(device_status_.storage_status().disks_size(), 1);
  const auto& disk = device_status_.storage_status().disks(0);
  EXPECT_EQ(disk.size(), kFakeStorageSize);
  EXPECT_EQ(disk.type(), kFakeStorageType);
  EXPECT_EQ(disk.manufacturer(), base::NumberToString(kFakeStorageManfid));
  EXPECT_EQ(disk.model(), kFakeStorageName);
  EXPECT_EQ(disk.serial(), base::NumberToString(kFakeStorageSerial));
  EXPECT_EQ(disk.bytes_read_since_last_boot(), kFakeStorageBytesRead);
  EXPECT_EQ(disk.bytes_written_since_last_boot(), kFakeStorageBytesWritten);
  EXPECT_EQ(disk.read_time_seconds_since_last_boot(),
            kFakeStorageReadTimeSeconds);
  EXPECT_EQ(disk.write_time_seconds_since_last_boot(),
            kFakeStorageWriteTimeSeconds);
  EXPECT_EQ(disk.io_time_seconds_since_last_boot(), kFakeStorageIoTimeSeconds);
  EXPECT_EQ(disk.discard_time_seconds_since_last_boot(),
            kFakeStorageDiscardTimeSeconds);
  ASSERT_TRUE(disk.has_emmc_oemid());
  EXPECT_EQ(disk.emmc_oemid(), kFakeOemid);
  ASSERT_TRUE(disk.has_emmc_pnm());
  EXPECT_EQ(disk.emmc_pnm(), kFakePnm);
  ASSERT_TRUE(disk.has_emmc_hardware_rev());
  EXPECT_EQ(disk.emmc_hardware_rev(), kFakePrv);
  ASSERT_TRUE(disk.has_emmc_firmware_rev());
  EXPECT_EQ(disk.emmc_firmware_rev(), kFakeFwrev);
  EXPECT_EQ(disk.purpose(), kFakeProtoPurpose);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdTimeZoneInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_timezone_info());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceTimezoneInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the Timezone info.
  ASSERT_TRUE(device_status_.has_timezone_info());
  EXPECT_EQ(device_status_.timezone_info().posix(), kPosixTimezone);
  EXPECT_EQ(device_status_.timezone_info().region(), kTimezoneRegion);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdFanInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_EQ(device_status_.fan_info_size(), 0);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceFanInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the fan info.
  ASSERT_EQ(device_status_.fan_info_size(), 1);
  const auto& fan = device_status_.fan_info(0);
  EXPECT_EQ(fan.speed_rpm(), kFakeSpeedRpm);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdBluetoothInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_EQ(device_status_.bluetooth_adapter_info_size(), 0);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceBluetoothInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the Bluetooth info.
  ASSERT_EQ(device_status_.bluetooth_adapter_info_size(), 1);
  const auto& adapter = device_status_.bluetooth_adapter_info(0);
  EXPECT_EQ(adapter.name(), kFakeBluetoothAdapterName);
  EXPECT_EQ(adapter.address(), kFakeBluetoothAdapterAddress);
  EXPECT_EQ(adapter.powered(), kFakeBluetoothAdapterIsPowered);
  EXPECT_EQ(adapter.num_connected_devices(), kFakeNumConnectedBluetoothDevices);
}

TEST_F(DeviceStatusCollectorTest, TestHealthdVersionInfo) {
  DisableDefaultSettings();
  SetFakeCrosHealthdData();
  GetStatus();
  EXPECT_FALSE(device_status_.has_tpm_version_info());

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVersionInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the Tpm info.
  ASSERT_TRUE(device_status_.has_tpm_version_info());
  const auto& tpm = device_status_.tpm_version_info();
  EXPECT_EQ(tpm.did_vid(), kFakeTpmDidVid);
}

TEST_F(DeviceStatusCollectorTest, TestCrosHealthdInfoOptional) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCpuInfo, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDevicePowerStatus, true);

  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->battery_result = CreateEmptyBatteryResult();
  telemetry_info->backlight_result = CreateEmptyBacklightResult();
  telemetry_info->fan_result = CreateEmptyFanResult();
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  GetStatus();

  // Verify the battery data is empty
  EXPECT_FALSE(device_status_.has_power_status());

  // Verify the backlight info is empty.
  EXPECT_EQ(device_status_.backlight_info_size(), 0);

  // Verify the fan info is empty.
  EXPECT_EQ(device_status_.fan_info_size(), 0);
}

TEST_F(DeviceStatusCollectorTest, TestUnsetTpmInfo) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVersionInfo, true);
  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  telemetry_info->tpm_result = CreatePartialTpmResult();
  GetStatus();

  // Verify the Tpm info is unset without crashing.
  ASSERT_TRUE(device_status_.has_tpm_version_info());
  const auto& tpm = device_status_.tpm_version_info();
  EXPECT_EQ(tpm.did_vid(), "");
}

TEST_F(DeviceStatusCollectorTest, TestPartialCrosHealthdInfo) {
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceCpuInfo, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDevicePowerStatus, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the CPU data.
  ASSERT_EQ(device_status_.cpu_info_size(), 1);
  const auto& cpu = device_status_.cpu_info(0);
  EXPECT_EQ(cpu.model_name(), kFakeModelName);
  EXPECT_EQ(cpu.architecture(), kFakeProtoArchitecture);
  EXPECT_EQ(cpu.max_clock_speed_khz(), kFakeMaxClockSpeed);

  // Verify the battery data.
  ASSERT_TRUE(device_status_.has_power_status());
  ASSERT_EQ(device_status_.power_status().batteries_size(), 1);
  const auto& battery = device_status_.power_status().batteries(0);
  EXPECT_EQ(battery.serial(), kFakeBatterySerial);
  EXPECT_EQ(battery.manufacturer(), kFakeBatteryVendor);
  EXPECT_EQ(battery.design_capacity(), kExpectedBatteryChargeFullDesign);
  EXPECT_EQ(battery.full_charge_capacity(), kExpectedBatteryChargeFull);
  EXPECT_EQ(battery.cycle_count(), kFakeBatteryCycleCount);
  EXPECT_EQ(battery.design_min_voltage(), kExpectedBatteryVoltageMinDesign);
  EXPECT_EQ(battery.manufacture_date(), kFakeSmartBatteryManufactureDate);
  EXPECT_EQ(battery.technology(), kFakeBatteryTechnology);

  // Verify the battery sample data.
  ASSERT_EQ(battery.samples_size(), 1);
  const auto& battery_sample = battery.samples(0);
  EXPECT_EQ(battery_sample.voltage(), kExpectedBatteryVoltageNow);
  EXPECT_EQ(battery_sample.remaining_capacity(), kExpectedBatteryChargeNow);
  EXPECT_EQ(battery_sample.temperature(),
            (kFakeSmartBatteryTemperature - kZeroCInDeciKelvin) / 10);
  EXPECT_EQ(battery_sample.current(), kExpectedBatteryCurrentNow);
  EXPECT_EQ(battery_sample.status(), kFakeBatteryStatus);

  EXPECT_FALSE(device_status_.has_memory_info());
  EXPECT_FALSE(device_status_.has_timezone_info());
  EXPECT_FALSE(device_status_.has_system_status());
  // Some smbios info from SystemResult is always reported by default.
  EXPECT_TRUE(device_status_.has_smbios_info());
  EXPECT_FALSE(device_status_.has_boot_info());
  EXPECT_FALSE(device_status_.has_storage_status());
  EXPECT_EQ(device_status_.backlight_info_size(), 0);
  EXPECT_EQ(device_status_.fan_info_size(), 0);
}

// Test ReportDeviceSystemInfo and ReportDeviceVpdInfo together because they
// are highly coupled.
TEST_F(DeviceStatusCollectorTest, TestCrosHealthdVpdAndSystemInfo) {
  // When the vpd reporting policy is turned on and the system reporting
  // property is turned off, we only expect the protobuf to only have vpd info.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSystemInfo, false);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVpdInfo, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the only vpd info is populated.
  ASSERT_TRUE(device_status_.has_system_status());
  EXPECT_EQ(device_status_.system_status().first_power_date(),
            kFakeVpdInfoFirstPowerDate);
  EXPECT_EQ(device_status_.system_status().manufacture_date(),
            kFakeVpdInfoManufactureDate);
  EXPECT_EQ(device_status_.system_status().vpd_sku_number(),
            kFakeVpdInfoSkuNumber);
  EXPECT_EQ(device_status_.system_status().vpd_serial_number(),
            kFakeVpdInfoSerialNumber);
  ASSERT_FALSE(device_status_.system_status().has_marketing_name());
  ASSERT_FALSE(device_status_.system_status().has_bios_version());
  ASSERT_FALSE(device_status_.system_status().has_board_name());
  ASSERT_FALSE(device_status_.system_status().has_board_version());
  ASSERT_FALSE(device_status_.system_status().has_chassis_type());
  ASSERT_FALSE(device_status_.system_status().has_product_name());

  // Verify the system info is not populated excluding vendor, product name and
  // product version.
  ASSERT_TRUE(device_status_.has_smbios_info());
  EXPECT_TRUE(device_status_.smbios_info().has_sys_vendor());
  EXPECT_TRUE(device_status_.smbios_info().has_product_name());
  EXPECT_TRUE(device_status_.smbios_info().has_product_version());
  EXPECT_FALSE(device_status_.smbios_info().has_bios_version());
  EXPECT_FALSE(device_status_.has_boot_info());

  // When the system reporting policy is turned on and the vpd reporting policy
  // is turned off, we expect the protobuf to have all system info except the
  // subset of vpd info.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSystemInfo, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVpdInfo, false);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify all system info except vpd info exists.
  ASSERT_TRUE(device_status_.has_system_status());
  ASSERT_FALSE(device_status_.system_status().has_first_power_date());
  ASSERT_FALSE(device_status_.system_status().has_manufacture_date());
  ASSERT_FALSE(device_status_.system_status().has_vpd_sku_number());
  EXPECT_EQ(device_status_.system_status().marketing_name(),
            kFakeOsInfoMarketingName);
  EXPECT_EQ(device_status_.system_status().bios_version(),
            kFakeDmiInfoBiosVersion);
  EXPECT_EQ(device_status_.system_status().board_name(), kFakeDmiInfoBoardName);
  EXPECT_EQ(device_status_.system_status().board_version(),
            kFakeDmiInfoBoardVersion);
  EXPECT_EQ(device_status_.system_status().chassis_type(),
            kFakeDmiInfoChassisType);
  EXPECT_EQ(device_status_.system_status().product_name(),
            kFakeOsInfoProductName);

  // Verify smbios info and boot info exist.
  ASSERT_TRUE(device_status_.has_smbios_info());
  EXPECT_EQ(device_status_.smbios_info().product_name(),
            kFakeDmiInfoProductName);
  EXPECT_EQ(device_status_.smbios_info().product_version(),
            kFakeDmiInfoProductVersion);
  EXPECT_EQ(device_status_.smbios_info().sys_vendor(), kFakeDmiInfoSysVendor);
  EXPECT_EQ(device_status_.smbios_info().bios_version(),
            kFakeDmiInfoBiosVersion);
  ASSERT_TRUE(device_status_.has_boot_info());
  EXPECT_EQ(device_status_.boot_info().boot_method(), kFakeOsInfoBootMethod);

  // Even with both settings off vendor, product name, and product version
  // should be reported.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceSystemInfo, false);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceVpdInfo, false);
  SetFakeCrosHealthdData();
  GetStatus();

  ASSERT_TRUE(device_status_.has_smbios_info());
  EXPECT_TRUE(device_status_.smbios_info().has_sys_vendor());
  EXPECT_TRUE(device_status_.smbios_info().has_product_name());
  EXPECT_TRUE(device_status_.smbios_info().has_product_version());
}

TEST_F(DeviceStatusCollectorTest, GenerateAppInfo) {
  DisableDefaultSettings();
  const AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
  MockRegularUserWithAffiliation(account_id, true);
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceAppInfo, true);
  managed_session_service_->OnUserProfileLoaded(account_id);
  auto* app_proxy =
      apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get());
  auto app1 = std::make_unique<apps::App>(apps::AppType::kChromeApp, "id");
  auto app2 = std::make_unique<apps::App>(apps::AppType::kChromeApp, "id2");
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app1));
  apps.push_back(std::move(app2));
  app_proxy->OnApps(std::move(apps), apps::AppType::kUnknown,
                    /*should_notify_initialized=*/false);
  apps::WaitForAppServiceProxyReady(app_proxy);

  // Start app instance
  base::Time start_time;
  EXPECT_TRUE(base::Time::FromString("29-MAR-2020 1:30pm", &start_time));
  test_clock_.SetNow(start_time);
  // Env::CreateInstance must be called for test window.
  auto env = aura::Env::CreateInstance();
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(/*id=*/0, nullptr));
  apps::InstanceParams params("id", window.get());
  params.state = std::make_pair(apps::InstanceState::kStarted, start_time);
  app_proxy->InstanceRegistry().CreateOrUpdateInstance(std::move(params));

  base::Time report_time;
  EXPECT_TRUE(base::Time::FromString("30-MAR-2020 2:30pm", &report_time));
  test_clock_.SetNow(report_time);
  GetStatus();

  base::Time reported_start_time;
  base::Time reported_end_time;
  EXPECT_EQ(session_status_.app_infos(0).app_id(), "id");
  EXPECT_EQ(session_status_.app_infos(0).active_time_periods_size(), 2);
  auto first_activity = session_status_.app_infos(0).active_time_periods()[0];
  EXPECT_TRUE(
      base::Time::FromUTCString("29-MAR-2020 12:00am", &reported_start_time));
  EXPECT_TRUE(
      base::Time::FromUTCString("29-MAR-2020 10:30am", &reported_end_time));
  EXPECT_EQ(first_activity.start_timestamp(),
            reported_start_time.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(first_activity.end_timestamp(),
            reported_end_time.InMillisecondsSinceUnixEpoch());
  auto second_activity = session_status_.app_infos(0).active_time_periods()[1];
  EXPECT_TRUE(
      base::Time::FromUTCString("30-MAR-2020 12:00am", &reported_start_time));
  EXPECT_TRUE(
      base::Time::FromUTCString("30-MAR-2020 2:30pm", &reported_end_time));
  EXPECT_EQ(second_activity.start_timestamp(),
            reported_start_time.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(second_activity.end_timestamp(),
            reported_end_time.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(session_status_.app_infos(1).app_id(), "id2");
  EXPECT_EQ(session_status_.app_infos(1).active_time_periods_size(), 0);
}

TEST_F(DeviceStatusCollectorTest, DemoModeDimensions) {
  enterprise_management::DemoModeDimensions expected;
  GetStatus();
  ash::test::AssertDemoDimensionsEqual(device_status_.demo_mode_dimensions(),
                                       expected);

  scoped_stub_install_attributes_.Get()->SetDemoMode();
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kFeatureManagementFeatureAwareDeviceDemoMode);
  scoped_local_state_.Get()->SetString(ash::prefs::kDemoModeCountry, "CA");
  scoped_local_state_.Get()->SetString(ash::prefs::kDemoModeRetailerId,
                                       "retailer");
  scoped_local_state_.Get()->SetString(ash::prefs::kDemoModeStoreId, "1234");

  expected.set_country("CA");
  expected.set_retailer_name("retailer");
  expected.set_store_number("1234");
  expected.add_customization_facets(
      enterprise_management::DemoModeDimensions::FEATURE_AWARE_DEVICE);

  GetStatus();
  ash::test::AssertDemoDimensionsEqual(device_status_.demo_mode_dimensions(),
                                       expected);
}

struct FakeSimSlotInfo {
  std::string object_path;
  std::string eid;
  bool is_active;
  uint32_t physical_slot;
} kFakeSimSlots[] = {{"euicc_path", "123", true, 1},
                     {"euicc_path2", "234", false, 2}};

// Fake device state.
struct FakeDeviceData {
  const char* device_path;
  const char* type;
  const char* object_path;
  const char* mac_address;
  const char* meid;
  const char* imei;
  const char* mdn;
  const char* iccid;
  int expected_type;  // proto enum type value, -1 for not present.
};

static const FakeDeviceData kFakeDevices[] = {
    {"/device/ethernet", shill::kTypeEthernet, "ethernet", "112233445566", "",
     "", "", "", em::NetworkInterface::TYPE_ETHERNET},
    {"/device/cellular1", shill::kTypeCellular, "cellular1", "abcdefabcdef",
     "A10000009296F2", "", "test_mdn", "test_iccid",
     em::NetworkInterface::TYPE_CELLULAR},
    {"/device/cellular2", shill::kTypeCellular, "cellular2", "abcdefabcdef",
     "test_mdn", "test_iccid", "352099001761481", "",
     em::NetworkInterface::TYPE_CELLULAR},
    {"/device/wifi", shill::kTypeWifi, "wifi", "aabbccddeeff", "", "", "", "",
     em::NetworkInterface::TYPE_WIFI},
    {"/device/vpn", shill::kTypeVPN, "vpn", "", "", "", "", "", -1},
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
    {"ethernet", "/device/ethernet", shill::kTypeEthernet, 0, 0,
     shill::kStateOnline, em::NetworkState::ONLINE, "192.168.0.1", "8.8.8.8",
     true},
    {"wifi", "/device/wifi", shill::kTypeWifi, 23, -77,
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
    {"failure", "/device/wifi", shill::kTypeWifi, 1, -87, shill::kStateFailure,
     em::NetworkState::FAILURE, "", "", true},
    {"unknown", "", shill::kTypeWifi, 1, -87, shill::kStateIdle,
     em::NetworkState::IDLE, "", "", true},
};

static const FakeNetworkState kUnconfiguredNetwork = {"unconfigured",
                                                      "/device/unconfigured",
                                                      shill::kTypeWifi,
                                                      35,
                                                      -85,
                                                      shill::kStateIdle,
                                                      em::NetworkState::IDLE,
                                                      "",
                                                      ""};

class DeviceStatusCollectorNetworkTest : public DeviceStatusCollectorTest {
 protected:
  void SetUp() override {
    RestartStatusCollector();

    ash::ShillDeviceClient::TestInterface* device_client =
        network_handler_test_helper_.device_test();
    ash::ShillServiceClient::TestInterface* service_client =
        network_handler_test_helper_.service_test();
    ash::ShillIPConfigClient::TestInterface* ip_config_client =
        network_handler_test_helper_.ip_config_test();

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
      if (*dev.mdn) {
        device_client->SetDeviceProperty(dev.device_path, shill::kMdnProperty,
                                         base::Value(dev.mdn),
                                         /*notify_changed=*/true);
      }
      if (*dev.iccid) {
        device_client->SetDeviceProperty(dev.device_path, shill::kIccidProperty,
                                         base::Value(dev.iccid),
                                         /*notify_changed=*/true);
      }
    }
    for (const auto& slotInfo : kFakeSimSlots) {
      ash::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
          dbus::ObjectPath(slotInfo.object_path), slotInfo.eid,
          slotInfo.is_active, slotInfo.physical_slot);
    }

    network_handler_test_helper_.profile_test()->AddProfile(
        kShillFakeProfilePath, kShillFakeUserhash);

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
        auto ip_config_properties =
            base::Value::Dict()
                .Set(shill::kAddressProperty, fake_network.address)
                .Set(shill::kGatewayProperty, fake_network.gateway);
        const std::string kIPConfigPath = "test_ip_config";
        ip_config_client->AddIPConfig(kIPConfigPath,
                                      std::move(ip_config_properties));
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

    ash::NetworkStateHandler::NetworkStateList state_list;
    ash::NetworkStateHandler* network_state_handler =
        ash::NetworkHandler::Get()->network_state_handler();
    network_state_handler->GetNetworkListByType(
        ash::NetworkTypePattern::Default(),
        true,   // configured_only
        false,  // visible_only,
        0,      // no limit to number of results
        &state_list);
    ASSERT_EQ(std::size(kFakeNetworks), state_list.size());
  }

  void TearDown() override { DeviceStatusCollectorTest::TearDown(); }

  void ClearNetworkData() {
    ash::ShillDeviceClient::TestInterface* device_client =
        network_handler_test_helper_.device_test();
    ash::ShillServiceClient::TestInterface* service_client =
        network_handler_test_helper_.service_test();

    device_client->ClearDevices();
    service_client->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  virtual void VerifyReporting() = 0;

 private:
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

class DeviceStatusCollectorNetworkInterfacesTest
    : public DeviceStatusCollectorNetworkTest {
 protected:
  void VerifyReporting() override {
    int count = 0;
    for (const FakeDeviceData& dev : kFakeDevices) {
      if (dev.expected_type == -1) {
        continue;
      }

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
            iface->has_mdn() == !!*dev.mdn &&
            iface->has_iccid() == !!*dev.iccid &&
            iface->mac_address() == dev.mac_address &&
            iface->meid() == dev.meid && iface->imei() == dev.imei &&
            iface->device_path() == dev.device_path &&
            iface->mdn() == dev.mdn && iface->iccid() == dev.iccid &&
            (iface->type() != em::NetworkInterface::TYPE_CELLULAR ||
             base::ranges::equal(iface->eids().begin(), iface->eids().end(),
                                 kFakeSimSlots,
                                 kFakeSimSlots + std::size(kFakeSimSlots),
                                 base::ranges::equal_to(), std::identity(),
                                 &FakeSimSlotInfo::eid))) {
          found_match = true;
          break;
        }
      }

      EXPECT_TRUE(found_match);
      count++;
    }

    EXPECT_EQ(count, device_status_.network_interfaces_size());
  }
};

// TODO(crbug.com/40793580): Revive this test.
TEST_F(DeviceStatusCollectorNetworkInterfacesTest, DISABLED_TestNoInterfaces) {
  ClearNetworkData();
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);

  // If no interfaces are found, nothing should be reported.
  GetStatus();
  EXPECT_EQ(device_status_.SerializeAsString(), "");
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, Default) {
  // Network interfaces should be reported by default, i.e if the policy is
  // not set.
  GetStatus();
  VerifyReporting();

  // Network interfaces should be reported if the policy is set to true.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  GetStatus();
  VerifyReporting();

  // Network interfaces should not be reported if the policy is set to false.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.network_interfaces_size());
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, IfUnaffiliatedUser) {
  // Network interfaces should be reported for unaffiliated users.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, false, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, IfAffiliatedUser) {
  // Network interfaces should be reported for affiliated users.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, IfPublicSession) {
  // Network interfaces should be reported if in public session.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddPublicAccountUser(
      AccountId::FromUserEmail(kPublicAccountId));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, IfKioskMode) {
  // Network interfaces should be reported if in kiosk mode.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  GetFakeChromeUserManager()->AddKioskAppUser(
      AccountId::FromUserEmail(kKioskAccountId));

  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, TestCrosHealthdBusInfo) {
  // Create a fake response from cros_healthd and populate it with some
  // test values.
  // Because this data collection is gated by the
  // kReportDeviceNetworkConfiguration policy, this test uses the
  // DeviceStatusCollectorNetworkInterfacesTest class.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);
  SetFakeCrosHealthdData();
  GetStatus();

  // Verify the Network Adapter (bus device) info.
  ASSERT_EQ(device_status_.network_adapter_info_size(), 2);
  // Verify the PCI device.
  const auto& network_adapter_0 = device_status_.network_adapter_info(0);
  EXPECT_EQ(network_adapter_0.vendor_name(), kFakePciVendor);
  EXPECT_EQ(network_adapter_0.device_name(), kFakePciProduct);
  EXPECT_EQ(network_adapter_0.device_class(), em::ETHERNET_CONTROLLER);
  EXPECT_EQ(network_adapter_0.bus_type(), em::PCI_BUS);
  EXPECT_EQ(network_adapter_0.driver_size(), 1);
  EXPECT_EQ(network_adapter_0.driver(0), kFakePciDriver);
  // Verify the USB device.
  const auto& network_adapter_1 = device_status_.network_adapter_info(1);
  EXPECT_EQ(network_adapter_1.vendor_name(), kFakeUsbVendor);
  EXPECT_EQ(network_adapter_1.device_name(), kFakeUsbProduct);
  EXPECT_EQ(network_adapter_1.device_class(), em::WIRELESS_CONTROLLER);
  EXPECT_EQ(network_adapter_1.bus_type(), em::USB_BUS);
  EXPECT_EQ(network_adapter_1.driver_size(), 2);
  EXPECT_EQ(network_adapter_1.driver(kFakeUsbInterfaceNumber0),
            kFakeUsbDriver0);
  EXPECT_EQ(network_adapter_1.driver(kFakeUsbInterfaceNumber1),
            kFakeUsbDriver1);
}

class DeviceStatusCollectorNetworkStateTest
    : public DeviceStatusCollectorNetworkTest {
 protected:
  void VerifyReporting() override {
    EXPECT_EQ(std::size(kFakeNetworks),
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
          if (proto_state.has_ip_address()) {
            EXPECT_EQ(proto_state.ip_address(), state.address);
          } else {
            EXPECT_EQ(0U, strlen(state.address));
          }
          if (proto_state.has_gateway()) {
            EXPECT_EQ(proto_state.gateway(), state.gateway);
          } else {
            EXPECT_EQ(0U, strlen(state.gateway));
          }
          found_match = true;
          break;
        }
      }
      EXPECT_TRUE(found_match) << "No matching state for fake network "
                               << " (" << state.name << ")";
    }
  }
};

TEST_F(DeviceStatusCollectorNetworkStateTest, Default) {
  // Network state should not be reported by default, i.e if the policy is not
  // set.
  GetStatus();
  EXPECT_EQ(0, device_status_.network_states_size());

  // Mock that the device is in kiosk mode to report network state.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  auto* user_manager = GetFakeChromeUserManager();
  auto* user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskAccountId));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();

  // Network state should not be reported if the policy is set to false.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, false);
  GetStatus();
  EXPECT_EQ(0, device_status_.network_states_size());

  // Network state should be reported if the policy is set to true.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  GetStatus();
  VerifyReporting();
}

// TODO(crbug.com/40793580): Revive this test.
TEST_F(DeviceStatusCollectorNetworkStateTest, DISABLED_TestNoNetworks) {
  ClearNetworkData();
  DisableDefaultSettings();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkConfiguration, true);

  // If no networks are found, nothing should be reported.
  GetStatus();
  EXPECT_EQ(device_status_.SerializeAsString(), "");
}

TEST_F(DeviceStatusCollectorNetworkStateTest, IfUnaffiliatedUser) {
  // Network state shouldn't be reported for unaffiliated users.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, false, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  GetStatus();
  EXPECT_EQ(0, device_status_.network_states_size());
}

TEST_F(DeviceStatusCollectorNetworkStateTest, IfAffiliatedUser) {
  // Network state should be reported for affiliated users.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  const AccountId account_id0(AccountId::FromUserEmail("user0@managed.com"));
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id0, true, user_manager::UserType::kRegular, nullptr);
  user_manager->UserLoggedIn(account_id0, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkStateTest, IfPublicSession) {
  // Network state should be reported if in public session.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  auto* user_manager = GetFakeChromeUserManager();
  auto* user = user_manager->AddPublicAccountUser(
      AccountId::FromUserEmail(kPublicAccountId));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();
}

TEST_F(DeviceStatusCollectorNetworkStateTest, IfKioskMode) {
  // Network state should be reported if in kiosk mode.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, true);
  auto* user_manager = GetFakeChromeUserManager();
  auto* user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskAccountId));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);

  GetStatus();
  VerifyReporting();
}

}  // namespace policy
