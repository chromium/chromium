// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"

#include <sys/types.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include "ash/components/audio/cras_audio_handler.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_reporting_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/status_collector/enterprise_activity_storage.h"
#include "chrome/browser/chromeos/policy/status_collector/interval_map.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector_state.h"
#include "chrome/browser/chromeos/policy/status_collector/tpm_status_combiner.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/enterprise_reporting.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/memory_stats.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

using base::Time;
using base::TimeDelta;

namespace em = enterprise_management;

namespace {

// How many seconds of inactivity triggers the idle state.
const int kIdleStateThresholdSeconds = 300;

// How much time in the past to store active periods for.
constexpr TimeDelta kMaxStoredPastActivityInterval = TimeDelta::FromDays(30);

// How much time in the future to store active periods for.
constexpr TimeDelta kMaxStoredFutureActivityInterval = TimeDelta::FromDays(2);

// How often, in seconds, to sample the hardware resource usage.
const unsigned int kResourceUsageSampleIntervalSeconds = 120;

// The location we read our CPU statistics from.
const char kProcStat[] = "/proc/stat";

// The location we read our CPU temperature and channel label from.
const char kHwmonDir[] = "/sys/class/hwmon/";
const char kDeviceDir[] = "device";
const char kHwmonDirectoryPattern[] = "hwmon*";
const char kCPUTempFilePattern[] = "temp*_input";

// The location where storage device statistics are read from.
const char kStorageInfoPath[] = "/var/log/storage_info.txt";

// The location where stateful partition info is read from.
const char kStatefulPartitionPath[] = "/home/.shadow";

// TODO(b/144081278): Remove when resolved.
// Debug values for cases when firmware version is not present.
const char kFirmwareFileEmpty[] = "FirmwareFileEmpty";
const char kFirmwareFileNotRead[] = "FirmwareFileNotRead";
const char kFirmwareNotInitialized[] = "FirmwareNotInitialized";
const char kFirmwareNotParsed[] = "FirmwareNotParsed";
// File to look for firmware number in.
const char kPathFirmware[] = "/var/log/bios_info.txt";

// O°C in deciKelvin.
const unsigned int kZeroCInDeciKelvin = 2731;

// The duration for crash report collection.
constexpr TimeDelta kCrashReportInfoDuration = TimeDelta::FromDays(1);

// The sources of crash report leads to device restart.
const char kCrashReportSourceKernel[] = "kernel";
const char kCrashReportSourceEC[] = "embedded-controller";

// The maximum number of crash report entries to be read.
// According to the official document, the crash reporter uploads no more than
// 24 MB (compressed) or 32 reports (whichever comes last) in any 24 hour
// window. Therefore, it looks safe to set max size as 100 here.
const int kCrashReportEntryMaxSize = 100;

// Helper function (invoked via blocking pool) to fetch information about
// mounted disks.
std::vector<em::VolumeInfo> GetVolumeInfo(
    const std::vector<std::string>& mount_points) {
  std::vector<em::VolumeInfo> result;
  for (const std::string& mount_point : mount_points) {
    base::FilePath mount_path(mount_point);

    // Non-native file systems do not have a mount point in the local file
    // system. However, it's worth checking here, as it's easier than checking
    // earlier which mount point is local, and which one is not.
    if (mount_point.empty() || !base::PathExists(mount_path))
      continue;

    int64_t free_size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    if (free_size < 0 || total_size < 0) {
      LOG(ERROR) << "Unable to get volume status for " << mount_point;
      continue;
    }
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    info.set_storage_total(total_size);
    info.set_storage_free(free_size);
    result.push_back(info);
  }
  return result;
}

// Reads the first CPU line from /proc/stat. Returns an empty string if
// the cpu data could not be read.
// The format of this line from /proc/stat is:
//
//   cpu  user_time nice_time system_time idle_time
//
// where user_time, nice_time, system_time, and idle_time are all integer
// values measured in jiffies from system startup.
std::string ReadCPUStatistics() {
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    size_t eol = contents.find("\n");
    if (eol != std::string::npos) {
      std::string line = contents.substr(0, eol);
      if (line.compare(0, 4, "cpu ") == 0)
        return line;
    }
    // First line should always start with "cpu ".
    NOTREACHED() << "Could not parse /proc/stat contents: " << contents;
  }
  LOG(WARNING) << "Unable to read CPU statistics from " << kProcStat;
  return std::string();
}

// Read system temperature sensor data into |out_contents| from
//
// |sensor_dir|/temp*_input
//
// which contains millidegree Celsius temperature and
//
// |sensor_dir|/temp*_label or
// |sensor_dir|/name
//
// which contains an appropriate label name for the given sensor.
// Returns |true| iff there was at least one sensor value in given |sensor_dir|.
bool ReadTemperatureSensorInfo(const base::FilePath& sensor_dir,
                               std::vector<em::CPUTempInfo>* out_contents) {
  bool has_data = false;

  base::FileEnumerator enumerator(
      sensor_dir, false, base::FileEnumerator::FILES, kCPUTempFilePattern);
  for (base::FilePath temperature_path = enumerator.Next();
       !temperature_path.empty(); temperature_path = enumerator.Next()) {
    // Get appropriate temp*_label file.
    std::string label_path = temperature_path.MaybeAsASCII();
    if (label_path.empty()) {
      LOG(WARNING) << "Unable to parse a path to temp*_input file as ASCII";
      continue;
    }
    base::ReplaceSubstringsAfterOffset(&label_path, 0, "input", "label");
    base::FilePath name_path = sensor_dir.Append("name");

    // Get the label describing this temperature. Use temp*_label
    // if present, fall back on name file or blank.
    std::string label;
    if (base::PathExists(base::FilePath(label_path))) {
      base::ReadFileToString(base::FilePath(label_path), &label);
    } else if (base::PathExists(base::FilePath(name_path))) {
      base::ReadFileToString(name_path, &label);
    } else {
      label = std::string();
    }

    // Read temperature in millidegree Celsius.
    std::string temperature_string;
    int32_t temperature = 0;
    if (base::ReadFileToString(temperature_path, &temperature_string) &&
        sscanf(temperature_string.c_str(), "%d", &temperature) == 1) {
      has_data = true;
      // CPU temp in millidegree Celsius to Celsius
      temperature /= 1000;

      em::CPUTempInfo info;
      info.set_cpu_label(label);
      info.set_cpu_temp(temperature);
      out_contents->push_back(info);
    } else {
      LOG(WARNING) << "Unable to read CPU temp from "
                   << temperature_path.MaybeAsASCII();
    }
  }
  return has_data;
}

// Read system temperature sensors from
//
// /sys/class/hwmon/hwmon*/(device/)?
std::vector<em::CPUTempInfo> ReadCPUTempInfo() {
  std::vector<em::CPUTempInfo> contents;
  // Get directories /sys/class/hwmon/hwmon*
  base::FileEnumerator hwmon_enumerator(base::FilePath(kHwmonDir), false,
                                        base::FileEnumerator::DIRECTORIES,
                                        kHwmonDirectoryPattern);
  for (base::FilePath hwmon_path = hwmon_enumerator.Next(); !hwmon_path.empty();
       hwmon_path = hwmon_enumerator.Next()) {
    // Get temp*_input files in hwmon*/ and hwmon*/device/
    base::FilePath device_path = hwmon_path.Append(kDeviceDir);
    if (base::PathExists(device_path)) {
      // We might have hwmon*/device/, but sensor values are still in hwmon*/
      if (!ReadTemperatureSensorInfo(device_path, &contents)) {
        ReadTemperatureSensorInfo(hwmon_path, &contents);
      }
    } else {
      ReadTemperatureSensorInfo(hwmon_path, &contents);
    }
  }
  return contents;
}

// If |contents| contains |prefix| followed by a hex integer, parses the hex
// integer of specified length and returns it.
// Otherwise, returns base::nullopt.
base::Optional<int> ExtractHexIntegerAfterPrefix(base::StringPiece contents,
                                                 base::StringPiece prefix,
                                                 size_t hex_number_length) {
  size_t prefix_position = contents.find(prefix);
  if (prefix_position == std::string::npos)
    return base::nullopt;
  if (prefix_position + prefix.size() + hex_number_length >= contents.size())
    return base::nullopt;
  int parsed_number;
  if (!base::HexStringToInt(
          contents.substr(prefix_position + prefix.size(), hex_number_length),
          &parsed_number)) {
    return base::nullopt;
  }
  return parsed_number;
}

// Read life time estimation value for eMMC from data generated by
// chromeos_disk_metrics. The data is stored in format:
// [DEVICE_LIFE_TIME_EST_TYP_[AB]: 0xXX]
// where A, B indicate the area of MMC being assesed(SLC and MLC), XX -- hex
// integer representing wear out of selected area.
// reference: e.MMC Device Health Report
// https://www.micron.com/products/managed-nand/emmc/emmc-software
em::DiskLifetimeEstimation ReadDiskLifeTimeEstimation() {
  em::DiskLifetimeEstimation est;
  std::string contents;
  const std::string pattern_slc = "[DEVICE_LIFE_TIME_EST_TYP_A: 0x";
  const std::string pattern_mlc = "[DEVICE_LIFE_TIME_EST_TYP_B: 0x";
  if (!base::ReadFileToStringWithMaxSize(
          base::FilePath(kStorageInfoPath), &contents,
          40000)) {  // max size in case somebody tackles with the file
    return est;
  }
  auto slc_est = ExtractHexIntegerAfterPrefix(contents, pattern_slc, 2);
  if (slc_est)
    est.set_slc(slc_est.value());
  auto mlc_est = ExtractHexIntegerAfterPrefix(contents, pattern_mlc, 2);
  if (mlc_est)
    est.set_mlc(mlc_est.value());
  return est;
}

// Read stateful partition info for user data.
em::StatefulPartitionInfo ReadStatefulPartitionInfo() {
  em::StatefulPartitionInfo spi;
  const base::FilePath statefulPartitionPath(kStatefulPartitionPath);
  const int64_t available_space =
      base::SysInfo::AmountOfFreeDiskSpace(statefulPartitionPath);
  const int64_t total_space =
      base::SysInfo::AmountOfTotalDiskSpace(statefulPartitionPath);

  if (available_space == -1) {
    LOG(ERROR) << "ReadStatefulPartitionInfo failed fetching available space.";
    return spi;
  }

  if (total_space == -1) {
    LOG(ERROR) << "ReadStatefulPartitionInfo failed fetching total space.";
    return spi;
  }

  spi.set_available_space(available_space);
  spi.set_total_space(total_space);
  return spi;
}

// Collects all the display related information.
void GetDisplayStatus(em::GraphicsStatus* graphics_status) {
  const std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    em::DisplayInfo* display_info = graphics_status->add_displays();
    display_info->set_resolution_width(display.GetSizeInPixel().width());
    display_info->set_resolution_height(display.GetSizeInPixel().height());
    display_info->set_refresh_rate(display.display_frequency());
    display_info->set_is_internal(display.IsInternal());
  }
}

// Makes the requested |gpu_memory_stats| available. Collects the other required
// graphics properties next. Finally, calls |callback|.
void OnVideoMemoryUsageStatsUpdate(
    policy::DeviceStatusCollector::GraphicsStatusReceiver callback,
    std::unique_ptr<em::GraphicsStatus> graphics_status,
    const gpu::VideoMemoryUsageStats& gpu_memory_stats) {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  gpu::GPUInfo gpu_info = gpu_data_manager->GetGPUInfo();
  // Adapter information
  em::GraphicsAdapterInfo* graphics_info = graphics_status->mutable_adapter();
  graphics_info->set_name(gpu_info.gpu.device_string);
  graphics_info->set_driver_version(gpu_info.gpu.driver_version);
  graphics_info->set_device_id(gpu_info.gpu.device_id);
  graphics_info->set_system_ram_usage(gpu_memory_stats.bytes_allocated);

  std::move(callback).Run(*graphics_status);
}

// Fetches display-related and graphics-adapter information.
void FetchGraphicsStatus(
    policy::DeviceStatusCollector::GraphicsStatusReceiver callback) {
  std::unique_ptr<em::GraphicsStatus> graphics_status =
      std::make_unique<em::GraphicsStatus>();
  GetDisplayStatus(graphics_status.get());
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  gpu_data_manager->RequestVideoMemoryUsageStatsUpdate(
      base::BindOnce(&OnVideoMemoryUsageStatsUpdate, std::move(callback),
                     std::move(graphics_status)));
}

bool ReadAndroidStatus(
    policy::StatusCollector::AndroidStatusReceiver receiver) {
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  auto* const instance_holder =
      arc_service_manager->arc_bridge_service()->enterprise_reporting();
  if (!instance_holder)
    return false;
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_holder, GetStatus);
  if (!instance)
    return false;
  instance->GetStatus(std::move(receiver));
  return true;
}

void ReadTpmStatus(policy::DeviceStatusCollector::TpmStatusReceiver callback) {
  // D-Bus calls are allowed only on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto tpm_status_combiner =
      base::MakeRefCounted<::policy::TpmStatusCombiner>(std::move(callback));
  chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&::policy::TpmStatusCombiner::OnGetTpmStatus,
                     tpm_status_combiner));
  chromeos::AttestationClient::Get()->GetStatus(
      ::attestation::GetStatusRequest(),
      base::BindOnce(&::policy::TpmStatusCombiner::OnGetEnrollmentStatus,
                     tpm_status_combiner));
  chromeos::TpmManagerClient::Get()->GetDictionaryAttackInfo(
      ::tpm_manager::GetDictionaryAttackInfoRequest(),
      base::BindOnce(&::policy::TpmStatusCombiner::OnGetDictionaryAttackInfo,
                     tpm_status_combiner));
}

base::Version GetPlatformVersion() {
  return base::Version(base::SysInfo::OperatingSystemVersion());
}

// Helper routine to convert from Shill-provided signal strength (percent)
// to dBm units expected by server.
int ConvertWifiSignalStrength(int signal_strength) {
  // Shill attempts to convert WiFi signal strength from its internal dBm to a
  // percentage range (from 0-100) by adding 120 to the raw dBm value,
  // and then clamping the result to the range 0-100 (see
  // shill::WiFiService::SignalToStrength()).
  //
  // To convert back to dBm, we subtract 120 from the percentage value to yield
  // a clamped dBm value in the range of -119 to -20dBm.
  //
  // TODO(atwilson): Tunnel the raw dBm signal strength from Shill instead of
  // doing the conversion here so we can report non-clamped values
  // (crbug.com/463334).
  DCHECK_GT(signal_strength, 0);
  DCHECK_LE(signal_strength, 100);
  return signal_strength - 120;
}

bool IsKioskApp() {
  return chromeos::LoginState::Get()->GetLoggedInUserType() ==
         chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP;
}

// Utility method to turn cpu_temp_fetcher_ to OnceCallback
std::vector<em::CPUTempInfo> InvokeCpuTempFetcher(
    policy::DeviceStatusCollector::CPUTempFetcher fetcher) {
  return fetcher.Run();
}

// Utility method to complete information for a reported Crostini App.
// Returns whether all required App information could be retrieved or not.
bool AddCrostiniAppInfo(
    const guest_os::GuestOsRegistryService::Registration& registration,
    em::CrostiniApp* const app) {
  app->set_app_name(registration.Name());
  const base::Time last_launch_time = registration.LastLaunchTime();
  if (!last_launch_time.is_null()) {
    app->set_last_launch_time_window_start_timestamp(
        crostini::GetThreeDayWindowStart(last_launch_time).ToJavaTime());
  }

  if (registration.app_id() == crostini::kCrostiniTerminalSystemAppId) {
    app->set_app_type(em::CROSTINI_APP_TYPE_TERMINAL);
    // We do not log package information if the App is the terminal:
    return true;
  }
  app->set_app_type(em::CROSTINI_APP_TYPE_INTERACTIVE);

  const std::string& package_id = registration.PackageId();
  if (package_id.empty())
    return true;

  const std::vector<std::string> package_info = base::SplitString(
      package_id, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // The package identifier is in the form of a semicolon delimited string of
  // the format: name;version;arch;data (see cicerone_service.proto)
  if (package_info.size() != 4) {
    LOG(ERROR) << "Package id has the wrong format: " << package_id;
    return false;
  }

  app->set_package_name(package_info[0]);
  app->set_package_version(package_info[1]);

  return true;
}

// Utility method to add a list of installed Crostini Apps to Crostini status
void AddCrostiniAppListForProfile(Profile* const profile,
                                  em::CrostiniStatus* const crostini_status) {
  const std::map<std::string, guest_os::GuestOsRegistryService::Registration>&
      registered_apps =
          guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
              ->GetRegisteredApps(guest_os::GuestOsRegistryService::VmType::
                                      ApplicationList_VmType_TERMINA);
  for (const auto& pair : registered_apps) {
    const std::string& registered_app_id = pair.first;
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    em::CrostiniApp* const app = crostini_status->add_installed_apps();
    if (!AddCrostiniAppInfo(registration, app)) {
      LOG(ERROR) << "Could not retrieve all required information for "
                    "registered app_id: "
                 << registered_app_id;
    }
  }
}

// Reads content of firmware file.
// Returns pair of the firmware version and fetch error if not fetched.
// TODO(b/144081278): Just call chromeos::version_loader::ParseFirmware() when
// it's resolved.
std::pair<std::string, std::string> ReadFirmwareVersion() {
  std::string firmware;
  std::string contents;
  const base::FilePath file_path(kPathFirmware);
  if (!base::ReadFileToString(file_path, &contents))
    return {firmware, kFirmwareFileNotRead};
  if (contents.empty())
    return {firmware, kFirmwareFileEmpty};
  firmware = chromeos::version_loader::ParseFirmware(contents);
  if (firmware.empty())
    return {firmware, kFirmwareNotParsed};
  return {firmware, std::string()};
}

em::CrashReportInfo::CrashReportUploadStatus GetCrashReportUploadStatus(
    UploadList::UploadInfo::State state) {
  switch (state) {
    case UploadList::UploadInfo::State::NotUploaded:
      return em::CrashReportInfo::UPLOAD_STATUS_NOT_UPLOADED;
    case UploadList::UploadInfo::State::Pending:
      return em::CrashReportInfo::UPLOAD_STATUS_PENDING;
    case UploadList::UploadInfo::State::Pending_UserRequested:
      return em::CrashReportInfo::UPLOAD_STATUS_PENDING_USER_REQUESTED;
    case UploadList::UploadInfo::State::Uploaded:
      return em::CrashReportInfo::UPLOAD_STATUS_UPLOADED;
    default:
      return em::CrashReportInfo::UPLOAD_STATUS_UNKNOWN;
  }

  NOTREACHED();
}

// Filter the loaded crash reports.
// - the |upload_time| should be with last 24 hours.
// - the |source| should be 'kernel' or 'embedded-controller'.
void CrashReportsLoaded(
    scoped_refptr<UploadList> upload_list,
    policy::DeviceStatusCollector::CrashReportInfoReceiver callback) {
  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(kCrashReportEntryMaxSize, &uploads);

  const Time end_time = Time::Now();
  const Time start_time = end_time - kCrashReportInfoDuration;

  std::vector<em::CrashReportInfo> contents;
  for (const UploadList::UploadInfo& crash_report : uploads) {
    if (crash_report.upload_time >= start_time &&
        crash_report.upload_time < end_time &&
        (crash_report.source == kCrashReportSourceKernel ||
         crash_report.source == kCrashReportSourceEC)) {
      em::CrashReportInfo info;
      info.set_remote_id(crash_report.upload_id);
      info.set_capture_timestamp(crash_report.capture_time.ToJavaTime());
      info.set_cause(crash_report.source);
      info.set_upload_status(GetCrashReportUploadStatus(crash_report.state));
      contents.push_back(info);
    }
  }

  std::move(callback).Run(contents);
}

// Read the crash reports stored in the uploads.log file.
void ReadCrashReportInfo(
    policy::DeviceStatusCollector::CrashReportInfoReceiver callback) {
  scoped_refptr<UploadList> upload_list = CreateCrashUploadList();
  upload_list->Load(
      base::BindOnce(CrashReportsLoaded, upload_list, std::move(callback)));
}

}  // namespace

namespace policy {

class DeviceStatusCollectorState : public StatusCollectorState {
 public:
  explicit DeviceStatusCollectorState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      StatusCollectorCallback response)
      : StatusCollectorState(task_runner, std::move(response)) {}

  // Queues an async callback to query disk volume information.
  void SampleVolumeInfo(
      const DeviceStatusCollector::VolumeInfoFetcher& volume_info_fetcher) {
    // Create list of mounted disk volumes to query status.
    std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
    storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
        &external_mount_points);

    std::vector<std::string> mount_points;
    for (const auto& info : external_mount_points)
      mount_points.push_back(info.path.value());

    for (const auto& mount_info :
         chromeos::disks::DiskMountManager::GetInstance()->mount_points()) {
      // Extract a list of mount points to populate.
      mount_points.push_back(mount_info.first);
    }

    // Call out to the blocking pool to sample disk volume info.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(volume_info_fetcher, mount_points),
        base::BindOnce(&DeviceStatusCollectorState::OnVolumeInfoReceived,
                       this));
  }

  // Queues an async callback to query CPU temperature information.
  void SampleCPUTempInfo(
      const DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher) {
    // Call out to the blocking pool to sample CPU temp.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(cpu_temp_fetcher),
        base::BindOnce(&DeviceStatusCollectorState::OnCPUTempInfoReceived,
                       this));
  }

  bool FetchAndroidStatus(
      const StatusCollector::AndroidStatusFetcher& android_status_fetcher) {
    return android_status_fetcher.Run(base::BindOnce(
        &DeviceStatusCollectorState::OnAndroidInfoReceived, this));
  }

  void FetchTpmStatus(
      const DeviceStatusCollector::TpmStatusFetcher& tpm_status_fetcher) {
    tpm_status_fetcher.Run(
        base::BindOnce(&DeviceStatusCollectorState::OnTpmStatusReceived, this));
  }

  void FetchCrosHealthdData(
      const policy::DeviceStatusCollector::CrosHealthdDataFetcher&
          cros_healthd_data_fetcher,
      bool report_system_info,
      bool report_vpd_info) {
    cros_healthd_data_fetcher.Run(
        CrosHealthdCollectionMode::kFull,
        base::BindOnce(&DeviceStatusCollectorState::OnCrosHealthdDataReceived,
                       this, report_system_info, report_vpd_info));
  }

  void FetchEMMCLifeTime(
      const policy::DeviceStatusCollector::EMMCLifetimeFetcher&
          emmc_lifetime_fetcher) {
    // Call out to the blocking pool to read disklifetimeestimation.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(emmc_lifetime_fetcher),
        base::BindOnce(&DeviceStatusCollectorState::OnEMMCLifetimeReceived,
                       this));
  }

  void FetchStatefulPartitionInfo(
      const policy::DeviceStatusCollector::StatefulPartitionInfoFetcher&
          stateful_partition_info_fetcher) {
    // Call out to the blocking pool to read stateful partition information.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(stateful_partition_info_fetcher),
        base::BindOnce(
            &DeviceStatusCollectorState::OnStatefulPartitionInfoReceived,
            this));
  }

  void FetchGraphicsStatus(
      const policy::DeviceStatusCollector::GraphicsStatusFetcher&
          graphics_status_fetcher) {
    graphics_status_fetcher.Run(base::BindOnce(
        &DeviceStatusCollectorState::OnGraphicsStatusReceived, this));
  }

  void FetchCrashReportInfo(
      const policy::DeviceStatusCollector::CrashReportInfoFetcher&
          crash_report_fetcher) {
    crash_report_fetcher.Run(base::BindOnce(
        &DeviceStatusCollectorState::OnCrashReportInfoReceived, this));
  }

 private:
  ~DeviceStatusCollectorState() override = default;

  void OnVolumeInfoReceived(const std::vector<em::VolumeInfo>& volume_info) {
    response_params_.device_status->clear_volume_infos();
    for (const em::VolumeInfo& info : volume_info)
      *response_params_.device_status->add_volume_infos() = info;
  }

  void OnCPUTempInfoReceived(
      const std::vector<em::CPUTempInfo>& cpu_temp_info) {
    // Only one of OnCrosHealthdDataReceived or OnCPUTempInfoReceived should be
    // called.
    DCHECK_EQ(response_params_.device_status->cpu_temp_infos_size(), 0);

    DLOG_IF(WARNING, cpu_temp_info.empty())
        << "Unable to read CPU temp information.";
    base::Time timestamp = base::Time::Now();
    for (const em::CPUTempInfo& info : cpu_temp_info) {
      auto* new_info = response_params_.device_status->add_cpu_temp_infos();
      *new_info = info;
      new_info->set_timestamp(timestamp.ToJavaTime());
    }
  }

  void OnAndroidInfoReceived(const std::string& status,
                             const std::string& droid_guard_info) {
    em::AndroidStatus* const android_status =
        response_params_.session_status->mutable_android_status();
    android_status->set_status_payload(status);
    android_status->set_droid_guard_info(droid_guard_info);
  }

  void OnTpmStatusReceived(const TpmStatusInfo& tpm_status_struct) {
    // Make sure we edit the state on the right thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    em::TpmStatusInfo* const tpm_status_proto =
        response_params_.device_status->mutable_tpm_status_info();

    tpm_status_proto->set_enabled(tpm_status_struct.enabled);
    tpm_status_proto->set_owned(tpm_status_struct.owned);
    tpm_status_proto->set_tpm_initialized(tpm_status_struct.initialized);
    tpm_status_proto->set_attestation_prepared(
        tpm_status_struct.attestation_prepared);
    tpm_status_proto->set_attestation_enrolled(
        tpm_status_struct.attestation_enrolled);
    tpm_status_proto->set_dictionary_attack_counter(
        tpm_status_struct.dictionary_attack_counter);
    tpm_status_proto->set_dictionary_attack_threshold(
        tpm_status_struct.dictionary_attack_threshold);
    tpm_status_proto->set_dictionary_attack_lockout_in_effect(
        tpm_status_struct.dictionary_attack_lockout_in_effect);
    tpm_status_proto->set_dictionary_attack_lockout_seconds_remaining(
        tpm_status_struct.dictionary_attack_lockout_seconds_remaining);
    tpm_status_proto->set_boot_lockbox_finalized(
        tpm_status_struct.boot_lockbox_finalized);
  }

  // Stores the contents of |probe_result| and |samples| to |response_params_|.
  void OnCrosHealthdDataReceived(
      bool report_system_info,
      bool report_vpd_info,
      chromeos::cros_healthd::mojom::TelemetryInfoPtr probe_result,
      const base::circular_deque<std::unique_ptr<SampledData>>& samples) {
    namespace cros_healthd = chromeos::cros_healthd::mojom;
    // Make sure we edit the state on the right thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Only one of OnCrosHealthdDataReceived or OnCPUTempInfoReceived should be
    // called.
    DCHECK_EQ(response_params_.device_status->cpu_temp_infos_size(), 0);

    // Store CPU measurement samples.
    for (const std::unique_ptr<SampledData>& sample_data : samples) {
      for (auto kv : sample_data->cpu_samples) {
        response_params_.device_status->mutable_cpu_temp_infos()->Add(
            std::move(kv.second));
      }
    }

    if (probe_result.is_null())
      return;

    // Process NonRemovableBlockDeviceResult.
    const auto& block_device_result = probe_result->block_device_result;
    if (!block_device_result.is_null()) {
      switch (block_device_result->which()) {
        case cros_healthd::NonRemovableBlockDeviceResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting block device info: "
                     << block_device_result->get_error()->msg;
          break;
        }

        case cros_healthd::NonRemovableBlockDeviceResult::Tag::
            BLOCK_DEVICE_INFO: {
          em::StorageStatus* const storage_status_out =
              response_params_.device_status->mutable_storage_status();
          for (const auto& storage :
               block_device_result->get_block_device_info()) {
            em::DiskInfo* const disk_info_out = storage_status_out->add_disks();
            disk_info_out->set_serial(base::NumberToString(storage->serial));
            disk_info_out->set_manufacturer(
                base::NumberToString(storage->manufacturer_id));
            disk_info_out->set_model(storage->name);
            disk_info_out->set_type(storage->type);
            disk_info_out->set_size(storage->size);
            disk_info_out->set_bytes_read_since_last_boot(
                storage->bytes_read_since_last_boot);
            disk_info_out->set_bytes_written_since_last_boot(
                storage->bytes_written_since_last_boot);
            disk_info_out->set_read_time_seconds_since_last_boot(
                storage->read_time_seconds_since_last_boot);
            disk_info_out->set_write_time_seconds_since_last_boot(
                storage->write_time_seconds_since_last_boot);
            disk_info_out->set_io_time_seconds_since_last_boot(
                storage->io_time_seconds_since_last_boot);
            const auto& discard_time =
                storage->discard_time_seconds_since_last_boot;
            if (!discard_time.is_null()) {
              disk_info_out->set_discard_time_seconds_since_last_boot(
                  discard_time->value);
            }

            // vendor_id
            const auto& vendor_id = storage->vendor_id;
            switch (vendor_id->which()) {
              case chromeos::cros_healthd::mojom::BlockDeviceVendor::Tag::
                  NVME_SUBSYSTEM_VENDOR:
                disk_info_out->set_nvme_subsystem_vendor(
                    vendor_id->get_nvme_subsystem_vendor());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceVendor::Tag::
                  EMMC_OEMID:
                disk_info_out->set_emmc_oemid(vendor_id->get_emmc_oemid());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceVendor::Tag::OTHER:
                disk_info_out->set_other_vendor(vendor_id->get_other());
                break;
            }

            // product_id
            const auto& product_id = storage->product_id;
            switch (product_id->which()) {
              case chromeos::cros_healthd::mojom::BlockDeviceProduct::Tag::
                  NVME_SUBSYSTEM_DEVICE:
                disk_info_out->set_nvme_subsystem_device(
                    product_id->get_nvme_subsystem_device());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceProduct::Tag::
                  EMMC_PNM:
                disk_info_out->set_emmc_pnm(product_id->get_emmc_pnm());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceProduct::Tag::
                  OTHER:
                disk_info_out->set_other_product(product_id->get_other());
                break;
            }

            // revision
            const auto& revision = storage->revision;
            switch (revision->which()) {
              case chromeos::cros_healthd::mojom::BlockDeviceRevision::Tag::
                  NVME_PCIE_REV:
                disk_info_out->set_nvme_hardware_rev(
                    revision->get_nvme_pcie_rev());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceRevision::Tag::
                  EMMC_PRV:
                disk_info_out->set_emmc_hardware_rev(revision->get_emmc_prv());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceRevision::Tag::
                  OTHER:
                disk_info_out->set_other_hardware_rev(revision->get_other());
                break;
            }

            // firmware version
            const auto& fw_version = storage->firmware_version;
            switch (fw_version->which()) {
              case chromeos::cros_healthd::mojom::BlockDeviceFirmware::Tag::
                  NVME_FIRMWARE_REV:
                disk_info_out->set_nvme_firmware_rev(
                    fw_version->get_nvme_firmware_rev());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceFirmware::Tag::
                  EMMC_FWREV:
                disk_info_out->set_emmc_firmware_rev(
                    fw_version->get_emmc_fwrev());
                break;
              case chromeos::cros_healthd::mojom::BlockDeviceFirmware::Tag::
                  OTHER:
                disk_info_out->set_other_firmware_rev(fw_version->get_other());
                break;
            }

            switch (storage->purpose) {
              case chromeos::cros_healthd::mojom::StorageDevicePurpose::
                  kUnknown:
                disk_info_out->set_purpose(em::DiskInfo::PURPOSE_UNKNOWN);
                break;
              case chromeos::cros_healthd::mojom::StorageDevicePurpose::
                  kBootDevice:
                disk_info_out->set_purpose(em::DiskInfo::PURPOSE_BOOT);
                break;
              case chromeos::cros_healthd::mojom::StorageDevicePurpose::
                  kSwapDevice:
                disk_info_out->set_purpose(em::DiskInfo::PURPOSE_SWAP);
                break;
            }
          }
          break;
        }
      }
    }

    // Process BatteryResult.
    const auto& battery_result = probe_result->battery_result;
    if (!battery_result.is_null()) {
      switch (battery_result->which()) {
        case cros_healthd::BatteryResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting battery info: "
                     << battery_result->get_error()->msg;
          break;
        }

        case cros_healthd::BatteryResult::Tag::BATTERY_INFO: {
          const auto& battery_info = battery_result->get_battery_info();
          // Device does not have a battery.
          if (battery_info.is_null())
            break;

          em::PowerStatus* const power_status_out =
              response_params_.device_status->mutable_power_status();
          em::BatteryInfo* const battery_info_out =
              power_status_out->add_batteries();
          battery_info_out->set_serial(battery_info->serial_number);
          battery_info_out->set_manufacturer(battery_info->vendor);
          battery_info_out->set_cycle_count(battery_info->cycle_count);
          battery_info_out->set_technology(battery_info->technology);
          // Convert Ah to mAh:
          battery_info_out->set_design_capacity(
              std::lround(battery_info->charge_full_design * 1000));
          battery_info_out->set_full_charge_capacity(
              std::lround(battery_info->charge_full * 1000));
          // Convert V to mV:
          battery_info_out->set_design_min_voltage(
              std::lround(battery_info->voltage_min_design * 1000));
          if (battery_info->manufacture_date) {
            battery_info_out->set_manufacture_date(
                battery_info->manufacture_date.value());
          }

          for (const std::unique_ptr<SampledData>& sample_data : samples) {
            auto it =
                sample_data->battery_samples.find(battery_info->model_name);
            if (it != sample_data->battery_samples.end())
              battery_info_out->add_samples()->CheckTypeAndMergeFrom(
                  it->second);
          }
          break;
        }
      }
    }

    // Process CpuResult.
    const auto& cpu_result = probe_result->cpu_result;
    if (!cpu_result.is_null()) {
      switch (cpu_result->which()) {
        case cros_healthd::CpuResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting CPU info: "
                     << cpu_result->get_error()->msg;
          break;
        }

        case cros_healthd::CpuResult::Tag::CPU_INFO: {
          const auto& cpu_info = cpu_result->get_cpu_info();

          if (cpu_info.is_null()) {
            LOG(ERROR) << "Null CpuInfo from cros_healthd";
            break;
          }

          long clock_ticks_per_second = sysconf(_SC_CLK_TCK);
          if (clock_ticks_per_second == -1 || clock_ticks_per_second == 0) {
            LOG(ERROR) << "Failed getting number of clock ticks per second";
            break;
          }

          em::GlobalCpuInfo* const global_cpu_info_out =
              response_params_.device_status->mutable_global_cpu_info();
          global_cpu_info_out->set_num_total_threads(
              cpu_info->num_total_threads);

          for (const auto& physical_cpu : cpu_info->physical_cpus) {
            if (physical_cpu.is_null())
              continue;

            em::CpuInfo* const cpu_info_out =
                response_params_.device_status->add_cpu_info();
            if (physical_cpu->model_name)
              cpu_info_out->set_model_name(physical_cpu->model_name.value());
            cpu_info_out->set_architecture(
                static_cast<em::CpuInfo::Architecture>(cpu_info->architecture));

            for (const auto& logical_cpu : physical_cpu->logical_cpus) {
              if (logical_cpu.is_null())
                continue;

              em::LogicalCpuInfo* const logical_cpu_info_out =
                  cpu_info_out->add_logical_cpus();
              logical_cpu_info_out->set_scaling_max_frequency_khz(
                  logical_cpu->scaling_max_frequency_khz);
              logical_cpu_info_out->set_scaling_current_frequency_khz(
                  logical_cpu->scaling_current_frequency_khz);
              logical_cpu_info_out->set_idle_time_seconds(
                  logical_cpu->idle_time_user_hz / clock_ticks_per_second);

              if (!cpu_info_out->has_max_clock_speed_khz()) {
                cpu_info_out->set_max_clock_speed_khz(
                    logical_cpu->max_clock_speed_khz);
              }

              for (const auto& c_state : logical_cpu->c_states) {
                if (c_state.is_null())
                  continue;

                em::CpuCStateInfo* const c_state_info_out =
                    logical_cpu_info_out->add_c_states();
                c_state_info_out->set_name(c_state->name);
                c_state_info_out->set_time_in_state_since_last_boot_us(
                    c_state->time_in_state_since_last_boot_us);
              }
            }
          }
          break;
        }
      }
    }

    // Process TimezoneResult.
    const auto& timezone_result = probe_result->timezone_result;
    if (!timezone_result.is_null()) {
      switch (timezone_result->which()) {
        case cros_healthd::TimezoneResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting timezone info: "
                     << timezone_result->get_error()->msg;
          break;
        }

        case cros_healthd::TimezoneResult::Tag::TIMEZONE_INFO: {
          const auto& timezone_info = timezone_result->get_timezone_info();
          em::TimezoneInfo* const timezone_info_out =
              response_params_.device_status->mutable_timezone_info();
          timezone_info_out->set_posix(timezone_info->posix);
          timezone_info_out->set_region(timezone_info->region);
          break;
        }
      }
    }

    // Process MemoryResult.
    const auto& memory_result = probe_result->memory_result;
    if (!memory_result.is_null()) {
      switch (memory_result->which()) {
        case cros_healthd::MemoryResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting memory info: "
                     << memory_result->get_error()->msg;
          break;
        }

        case cros_healthd::MemoryResult::Tag::MEMORY_INFO: {
          const auto& memory_info = memory_result->get_memory_info();
          em::MemoryInfo* const memory_info_out =
              response_params_.device_status->mutable_memory_info();
          memory_info_out->set_total_memory_kib(memory_info->total_memory_kib);
          memory_info_out->set_free_memory_kib(memory_info->free_memory_kib);
          memory_info_out->set_available_memory_kib(
              memory_info->available_memory_kib);
          memory_info_out->set_page_faults_since_last_boot(
              memory_info->page_faults_since_last_boot);
          break;
        }
      }
    }

    // Process BacklightResult.
    const auto& backlight_result = probe_result->backlight_result;
    if (!backlight_result.is_null()) {
      switch (backlight_result->which()) {
        case cros_healthd::BacklightResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting backlight info: "
                     << backlight_result->get_error()->msg;
          break;
        }

        case cros_healthd::BacklightResult::Tag::BACKLIGHT_INFO: {
          for (const auto& backlight : backlight_result->get_backlight_info()) {
            em::BacklightInfo* const backlight_info_out =
                response_params_.device_status->add_backlight_info();
            backlight_info_out->set_path(backlight->path);
            backlight_info_out->set_max_brightness(backlight->max_brightness);
            backlight_info_out->set_brightness(backlight->brightness);
          }
          break;
        }
      }
    }

    // Process FanResult.
    const auto& fan_result = probe_result->fan_result;
    if (!fan_result.is_null()) {
      switch (fan_result->which()) {
        case cros_healthd::FanResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting fan info: "
                     << fan_result->get_error()->msg;
          break;
        }

        case cros_healthd::FanResult::Tag::FAN_INFO: {
          for (const auto& fan : fan_result->get_fan_info()) {
            em::FanInfo* const fan_info_out =
                response_params_.device_status->add_fan_info();
            fan_info_out->set_speed_rpm(fan->speed_rpm);
          }
          break;
        }
      }
    }

    // Process Bluetooth result.
    const auto& bluetooth_result = probe_result->bluetooth_result;
    if (!bluetooth_result.is_null()) {
      switch (bluetooth_result->which()) {
        case cros_healthd::BluetoothResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting Bluetooth info: "
                     << bluetooth_result->get_error()->msg;
          break;
        }

        case cros_healthd::BluetoothResult::Tag::BLUETOOTH_ADAPTER_INFO: {
          for (const auto& adapter :
               bluetooth_result->get_bluetooth_adapter_info()) {
            em::BluetoothAdapterInfo* const adapter_info_out =
                response_params_.device_status->add_bluetooth_adapter_info();
            adapter_info_out->set_name(adapter->name);
            adapter_info_out->set_address(adapter->address);
            adapter_info_out->set_powered(adapter->powered);
            adapter_info_out->set_num_connected_devices(
                adapter->num_connected_devices);
          }
          break;
        }
      }
    }

    // Process SystemResult.
    const auto& system_result = probe_result->system_result;
    if (!system_result.is_null()) {
      switch (system_result->which()) {
        case cros_healthd::SystemResult::Tag::ERROR: {
          LOG(ERROR) << "cros_healthd: Error getting system info: "
                     << system_result->get_error()->msg;
          break;
        }

        case cros_healthd::SystemResult::Tag::SYSTEM_INFO: {
          const auto& system_info = system_result->get_system_info();
          em::SystemStatus* const system_status_out =
              response_params_.device_status->mutable_system_status();
          if (report_vpd_info) {
            if (system_info->first_power_date.has_value()) {
              system_status_out->set_first_power_date(
                  system_info->first_power_date.value());
            }
            if (system_info->manufacture_date.has_value()) {
              system_status_out->set_manufacture_date(
                  system_info->manufacture_date.value());
            }
            if (system_info->product_sku_number.has_value()) {
              system_status_out->set_vpd_sku_number(
                  system_info->product_sku_number.value());
            }
            if (system_info->product_serial_number.has_value()) {
              system_status_out->set_vpd_serial_number(
                  system_info->product_serial_number.value());
            }
          }
          if (report_system_info) {
            system_status_out->set_marketing_name(system_info->marketing_name);
            if (system_info->bios_version.has_value()) {
              system_status_out->set_bios_version(
                  system_info->bios_version.value());
            }
            if (system_info->board_name.has_value()) {
              system_status_out->set_board_name(
                  system_info->board_name.value());
            }
            if (system_info->board_version.has_value()) {
              system_status_out->set_board_version(
                  system_info->board_version.value());
            }
            if (system_info->chassis_type) {
              system_status_out->set_chassis_type(
                  system_info->chassis_type->value);
            }
            if (system_info->product_name.has_value()) {
              system_status_out->set_product_name(
                  system_info->product_name.value());
            }
          }
          break;
        }
      }
    }
  }

  void OnEMMCLifetimeReceived(const em::DiskLifetimeEstimation& est) {
    if (!est.has_slc() && !est.has_mlc())
      return;
    em::DiskLifetimeEstimation* state =
        response_params_.device_status->mutable_storage_status()
            ->mutable_lifetime_estimation();
    state->CopyFrom(est);
  }

  void OnStatefulPartitionInfoReceived(const em::StatefulPartitionInfo& hdsi) {
    if (!hdsi.has_available_space() && !hdsi.has_total_space())
      return;
    em::StatefulPartitionInfo* stateful_partition_info =
        response_params_.device_status->mutable_stateful_partition_info();
    DCHECK_GE(hdsi.available_space(), 0);
    DCHECK_GE(hdsi.total_space(), hdsi.available_space());
    stateful_partition_info->CopyFrom(hdsi);
  }

  void OnGraphicsStatusReceived(const em::GraphicsStatus& gs) {
    *response_params_.device_status->mutable_graphics_status() = gs;
  }

  void OnCrashReportInfoReceived(
      const std::vector<em::CrashReportInfo>& crash_report_infos) {
    DCHECK(response_params_.device_status->crash_report_infos_size() == 0);
    for (const em::CrashReportInfo& info : crash_report_infos) {
      *response_params_.device_status->add_crash_report_infos() = info;
    }
  }
};

TpmStatusInfo::TpmStatusInfo() = default;
TpmStatusInfo::TpmStatusInfo(const TpmStatusInfo&) = default;
TpmStatusInfo::TpmStatusInfo(
    bool enabled,
    bool owned,
    bool initialized,
    bool attestation_prepared,
    bool attestation_enrolled,
    int32_t dictionary_attack_counter,
    int32_t dictionary_attack_threshold,
    bool dictionary_attack_lockout_in_effect,
    int32_t dictionary_attack_lockout_seconds_remaining,
    bool boot_lockbox_finalized)
    : enabled(enabled),
      owned(owned),
      initialized(initialized),
      attestation_prepared(attestation_prepared),
      attestation_enrolled(attestation_enrolled),
      dictionary_attack_counter(dictionary_attack_counter),
      dictionary_attack_threshold(dictionary_attack_threshold),
      dictionary_attack_lockout_in_effect(dictionary_attack_lockout_in_effect),
      dictionary_attack_lockout_seconds_remaining(
          dictionary_attack_lockout_seconds_remaining),
      boot_lockbox_finalized(boot_lockbox_finalized) {}
TpmStatusInfo::~TpmStatusInfo() = default;

SampledData::SampledData() = default;
SampledData::~SampledData() = default;

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* pref_service,
    chromeos::system::StatisticsProvider* provider,
    const VolumeInfoFetcher& volume_info_fetcher,
    const CPUStatisticsFetcher& cpu_statistics_fetcher,
    const CPUTempFetcher& cpu_temp_fetcher,
    const AndroidStatusFetcher& android_status_fetcher,
    const TpmStatusFetcher& tpm_status_fetcher,
    const EMMCLifetimeFetcher& emmc_lifetime_fetcher,
    const StatefulPartitionInfoFetcher& stateful_partition_info_fetcher,
    const CrosHealthdDataFetcher& cros_healthd_data_fetcher,
    const GraphicsStatusFetcher& graphics_status_fetcher,
    const CrashReportInfoFetcher& crash_report_info_fetcher,
    base::Clock* clock)
    : StatusCollector(provider, ash::CrosSettings::Get(), clock),
      pref_service_(pref_service),
      firmware_fetch_error_(kFirmwareNotInitialized),
      volume_info_fetcher_(volume_info_fetcher),
      cpu_statistics_fetcher_(cpu_statistics_fetcher),
      cpu_temp_fetcher_(cpu_temp_fetcher),
      android_status_fetcher_(android_status_fetcher),
      tpm_status_fetcher_(tpm_status_fetcher),
      emmc_lifetime_fetcher_(emmc_lifetime_fetcher),
      stateful_partition_info_fetcher_(stateful_partition_info_fetcher),
      cros_healthd_data_fetcher_(cros_healthd_data_fetcher),
      graphics_status_fetcher_(graphics_status_fetcher),
      crash_report_info_fetcher_(crash_report_info_fetcher),
      power_manager_(chromeos::PowerManagerClient::Get()),
      app_info_generator_(kMaxStoredPastActivityInterval, clock_) {
  // protected fields of `StatusCollector`.
  max_stored_past_activity_interval_ = kMaxStoredPastActivityInterval;
  max_stored_future_activity_interval_ = kMaxStoredFutureActivityInterval;

  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_ = base::SequencedTaskRunnerHandle::Get();

  if (volume_info_fetcher_.is_null())
    volume_info_fetcher_ = base::BindRepeating(&GetVolumeInfo);

  if (cpu_statistics_fetcher_.is_null())
    cpu_statistics_fetcher_ = base::BindRepeating(&ReadCPUStatistics);

  if (cpu_temp_fetcher_.is_null())
    cpu_temp_fetcher_ = base::BindRepeating(&ReadCPUTempInfo);

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::BindRepeating(&ReadAndroidStatus);

  if (tpm_status_fetcher_.is_null())
    tpm_status_fetcher_ = base::BindRepeating(&ReadTpmStatus);

  if (emmc_lifetime_fetcher_.is_null())
    emmc_lifetime_fetcher_ = base::BindRepeating(&ReadDiskLifeTimeEstimation);

  if (stateful_partition_info_fetcher_.is_null()) {
    stateful_partition_info_fetcher_ =
        base::BindRepeating(&ReadStatefulPartitionInfo);
  }

  if (cros_healthd_data_fetcher_.is_null()) {
    cros_healthd_data_fetcher_ =
        base::BindRepeating(&DeviceStatusCollector::FetchCrosHealthdData,
                            weak_factory_.GetWeakPtr());
  }

  if (graphics_status_fetcher_.is_null())
    graphics_status_fetcher_ = base::BindRepeating(&FetchGraphicsStatus);

  if (crash_report_info_fetcher_.is_null())
    crash_report_info_fetcher_ = base::BindRepeating(&ReadCrashReportInfo);

  idle_poll_timer_.Start(FROM_HERE, kIdlePollInterval, this,
                         &DeviceStatusCollector::CheckIdleState);
  resource_usage_sampling_timer_.Start(
      FROM_HERE, TimeDelta::FromSeconds(kResourceUsageSampleIntervalSeconds),
      this, &DeviceStatusCollector::SampleResourceUsage);

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  auto callback = base::BindRepeating(
      &DeviceStatusCollector::UpdateReportingSettings, base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVersionInfo, callback);
  activity_times_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceActivityTimes, callback);
  boot_mode_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBootMode, callback);
  network_interfaces_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceNetworkInterfaces, callback);
  users_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceUsers, callback);
  hardware_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceHardwareStatus, callback);
  session_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceSessionStatus, callback);
  os_update_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportOsUpdateStatus, callback);
  running_kiosk_app_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportRunningKioskApp, callback);
  power_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDevicePowerStatus, callback);
  storage_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceStorageStatus, callback);
  board_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBoardStatus, callback);
  cpu_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceCpuInfo, callback);
  graphics_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceGraphicsStatus, callback);
  timezone_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceTimezoneInfo, callback);
  memory_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceMemoryInfo, callback);
  backlight_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBacklightInfo, callback);
  crash_report_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceCrashReportInfo, callback);
  bluetooth_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBluetoothInfo, callback);
  fan_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceFanInfo, callback);
  vpd_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVpdInfo, callback);
  app_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceAppInfo, callback);
  system_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceSystemInfo, callback);
  stats_reporting_pref_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kStatsReportingPref, callback);

  affiliated_session_service_.AddObserver(&app_info_generator_);

  power_manager_->AddObserver(this);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS, firmware, and TPM version info.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&DeviceStatusCollector::OnOSVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadFirmwareVersion),
      base::BindOnce(&DeviceStatusCollector::OnOSFirmware,
                     weak_factory_.GetWeakPtr()));
  chromeos::TpmManagerClient::Get()->GetVersionInfo(
      ::tpm_manager::GetVersionInfoRequest(),
      base::BindOnce(&DeviceStatusCollector::OnGetTpmVersion,
                     weak_factory_.GetWeakPtr()));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kReportingUsers,
      base::BindRepeating(&DeviceStatusCollector::ReportingUsersChanged,
                          weak_factory_.GetWeakPtr()));

  DCHECK(pref_service_->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);
  activity_storage_ = std::make_unique<EnterpriseActivityStorage>(
      pref_service_, prefs::kDeviceActivityTimes);
}

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* pref_service,
    chromeos::system::StatisticsProvider* provider)
    : DeviceStatusCollector(
          pref_service,
          provider,
          DeviceStatusCollector::VolumeInfoFetcher(),
          DeviceStatusCollector::CPUStatisticsFetcher(),
          DeviceStatusCollector::CPUTempFetcher(),
          StatusCollector::AndroidStatusFetcher(),
          DeviceStatusCollector::TpmStatusFetcher(),
          DeviceStatusCollector::EMMCLifetimeFetcher(),
          DeviceStatusCollector::StatefulPartitionInfoFetcher(),
          DeviceStatusCollector::CrosHealthdDataFetcher(),
          DeviceStatusCollector::GraphicsStatusFetcher(),
          DeviceStatusCollector::CrashReportInfoFetcher()) {}

DeviceStatusCollector::~DeviceStatusCollector() {
  power_manager_->RemoveObserver(this);
  affiliated_session_service_.RemoveObserver(&app_info_generator_);
}

// static
constexpr base::TimeDelta DeviceStatusCollector::kIdlePollInterval;

// static
void DeviceStatusCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceActivityTimes);
}

void DeviceStatusCollector::CheckIdleState() {
  ProcessIdleState(ui::CalculateIdleState(kIdleStateThresholdSeconds));
}

void DeviceStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&DeviceStatusCollector::UpdateReportingSettings,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  // Keep the default values in sync with DeviceReportingProto in
  // components/policy/proto/chrome_device_policy.proto.
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceVersionInfo,
                                  &report_version_info_)) {
    report_version_info_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceActivityTimes,
                                  &report_activity_times_)) {
    report_activity_times_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBootMode,
                                  &report_boot_mode_)) {
    report_boot_mode_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceSessionStatus,
                                  &report_kiosk_session_status_)) {
    report_kiosk_session_status_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceNetworkInterfaces,
                                  &report_network_interfaces_)) {
    report_network_interfaces_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceUsers,
                                  &report_users_)) {
    report_users_ = true;
  }
  const bool already_reporting_hardware_status = report_hardware_status_;
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceHardwareStatus,
                                  &report_hardware_status_)) {
    report_hardware_status_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDevicePowerStatus,
                                  &report_power_status_)) {
    report_power_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceStorageStatus,
                                  &report_storage_status_)) {
    report_storage_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBoardStatus,
                                  &report_board_status_)) {
    report_board_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceCpuInfo,
                                  &report_cpu_info_)) {
    report_cpu_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceGraphicsStatus,
                                  &report_graphics_status_)) {
    report_graphics_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceTimezoneInfo,
                                  &report_timezone_info_)) {
    report_timezone_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceMemoryInfo,
                                  &report_memory_info_)) {
    report_memory_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBacklightInfo,
                                  &report_backlight_info_)) {
    report_backlight_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceCrashReportInfo,
                                  &report_crash_report_info_)) {
    report_crash_report_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBluetoothInfo,
                                  &report_bluetooth_info_)) {
    report_bluetooth_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceSystemInfo,
                                  &report_system_info_)) {
    report_system_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceFanInfo,
                                  &report_fan_info_)) {
    report_fan_info_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceVpdInfo,
                                  &report_vpd_info_)) {
    report_vpd_info_ = false;
  }
  report_app_info_ = false;
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceAppInfo,
                                  &report_app_info_)) {
    report_app_info_ = false;
  }
  app_info_generator_.OnReportingChanged(report_app_info_);
  if (!cros_settings_->GetBoolean(chromeos::kStatsReportingPref,
                                  &stat_reporting_pref_)) {
    stat_reporting_pref_ = false;
  }

  if (!report_hardware_status_) {
    ClearCachedResourceUsage();
  } else if (!already_reporting_hardware_status) {
    // Turning on hardware status reporting - fetch an initial sample
    // immediately instead of waiting for the sampling timer to fire.
    SampleResourceUsage();
  }

  // Os update status and running kiosk app reporting are disabled by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportOsUpdateStatus,
                                  &report_os_update_status_)) {
    report_os_update_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportRunningKioskApp,
                                  &report_running_kiosk_app_)) {
    report_running_kiosk_app_ = false;
  }
}

void DeviceStatusCollector::ClearCachedResourceUsage() {
  resource_usage_.clear();
  last_cpu_active_ = 0;
  last_cpu_idle_ = 0;
}

void DeviceStatusCollector::ProcessIdleState(ui::IdleState state) {
  // Do nothing if device activity reporting is disabled.
  if (!report_activity_times_)
    return;

  Time now = clock_->Now();

  // For kiosk apps we report total uptime instead of active time.
  if (state == ui::IDLE_STATE_ACTIVE || IsKioskApp()) {
    std::string user_email = GetUserForActivityReporting();
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    TimeDelta active_seconds = now - last_idle_check_;
    Time start;
    if (active_seconds < base::TimeDelta::FromSeconds(0) ||
        active_seconds >= 2 * kIdlePollInterval || last_idle_check_.is_null()) {
      start = now - kIdlePollInterval;
    } else {
      start = last_idle_check_;
    }
    activity_storage_->AddActivityPeriod(start, now, user_email);

    activity_storage_->PruneActivityPeriods(
        now, max_stored_past_activity_interval_,
        max_stored_future_activity_interval_);
  }
  last_idle_check_ = now;
}

void DeviceStatusCollector::PowerChanged(
    const power_manager::PowerSupplyProperties& prop) {
  if (!power_status_callback_.is_null())
    std::move(power_status_callback_).Run(prop);
}

void DeviceStatusCollector::SampleResourceUsage() {
  // Results must be written in the creation thread since that's where they
  // are read from in the Get*StatusAsync methods.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If hardware reporting has been disabled, do nothing here.
  if (!report_hardware_status_)
    return;

  // Call out to the blocking pool to sample CPU stats.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(cpu_statistics_fetcher_),
      base::BindOnce(&DeviceStatusCollector::ReceiveCPUStatistics,
                     weak_factory_.GetWeakPtr()));
}

void DeviceStatusCollector::ReceiveCPUStatistics(const std::string& stats) {
  int cpu_usage_percent = 0;
  if (stats.empty()) {
    DLOG(WARNING) << "Unable to read CPU statistics";
  } else {
    // Parse the data from /proc/stat, whose format is defined at
    // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.
    //
    // The CPU usage values in /proc/stat are measured in the imprecise unit
    // "jiffies", but we just care about the relative magnitude of "active" vs
    // "idle" so the exact value of a jiffy is irrelevant.
    //
    // An example value for this line:
    //
    // cpu 123 456 789 012 345 678
    //
    // We only care about the first four numbers: user_time, nice_time,
    // sys_time, and idle_time.
    uint64_t user = 0, nice = 0, system = 0, idle = 0;
    int vals = sscanf(stats.c_str(),
                      "cpu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &user,
                      &nice, &system, &idle);
    DCHECK_EQ(4, vals);

    // The values returned from /proc/stat are cumulative totals, so calculate
    // the difference between the last sample and this one.
    uint64_t active = user + nice + system;
    uint64_t total = active + idle;
    uint64_t last_total = last_cpu_active_ + last_cpu_idle_;
    DCHECK_GE(active, last_cpu_active_);
    DCHECK_GE(idle, last_cpu_idle_);
    DCHECK_GE(total, last_total);

    if ((total - last_total) > 0) {
      cpu_usage_percent =
          (100 * (active - last_cpu_active_)) / (total - last_total);
    }
    last_cpu_active_ = active;
    last_cpu_idle_ = idle;
  }

  DCHECK_LE(cpu_usage_percent, 100);

  // This timestamp is used in both ResourceUsage and SampledData for CPU
  // termporary, which is expected to be same according to existing
  // implementation.
  const base::Time timestamp = base::Time::Now();

  ResourceUsage usage = {cpu_usage_percent,
                         base::SysInfo::AmountOfAvailablePhysicalMemory(),
                         timestamp};

  resource_usage_.push_back(usage);

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (resource_usage_.size() > kMaxResourceUsageSamples)
    resource_usage_.pop_front();

  std::unique_ptr<SampledData> sample = std::make_unique<SampledData>();
  sample->timestamp = timestamp;

  if (report_power_status_) {
    cros_healthd_data_fetcher_.Run(CrosHealthdCollectionMode::kBattery,
                                   base::DoNothing());
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&InvokeCpuTempFetcher, cpu_temp_fetcher_),
        base::BindOnce(&DeviceStatusCollector::ReceiveCPUTemperature,
                       weak_factory_.GetWeakPtr(), std::move(sample),
                       SamplingCallback()));
  }
}

void DeviceStatusCollector::SampleProbeData(
    std::unique_ptr<SampledData> sample,
    SamplingProbeResultCallback callback,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result.is_null())
    return;

  const auto& battery_result = result->battery_result;
  if (!battery_result.is_null()) {
    if (battery_result->is_error()) {
      LOG(ERROR) << "cros_healthd: Error getting battery info: "
                 << battery_result->get_error()->msg;
    } else if (!battery_result->get_battery_info().is_null()) {
      const auto& battery = battery_result->get_battery_info();
      enterprise_management::BatterySample battery_sample;
      battery_sample.set_timestamp(sample->timestamp.ToJavaTime());
      // Convert V to mV:
      battery_sample.set_voltage(std::lround(battery->voltage_now * 1000));
      // Convert Ah to mAh:
      battery_sample.set_remaining_capacity(
          std::lround(battery->charge_now * 1000));
      // Convert A to mA:
      battery_sample.set_current(std::lround(battery->current_now * 1000));
      battery_sample.set_status(battery->status);
      // Convert 0.1 Kelvin to Celsius:
      if (battery->temperature) {
        battery_sample.set_temperature(
            (battery->temperature->value - kZeroCInDeciKelvin) / 10);
      }
      sample->battery_samples[battery->model_name] = battery_sample;
    }
  }

  SamplingCallback completion_callback;
  if (!callback.is_null()) {
    completion_callback =
        base::BindOnce(std::move(callback), std::move(result));
  }

  // PowerManagerClient::Observer::PowerChanged can be called as a result of
  // power_manager_->RequestStatusUpdate() as well as for other reasons,
  // so we store power_status_callback_ here instead of triggering
  // SampleDischargeRate from PowerChanged().
  DCHECK(power_status_callback_.is_null());  // Previous sampling is completed.

  power_status_callback_ = base::BindOnce(
      &DeviceStatusCollector::SampleDischargeRate, weak_factory_.GetWeakPtr(),
      std::move(sample), std::move(completion_callback));
  power_manager_->RequestStatusUpdate();
}

void DeviceStatusCollector::SampleDischargeRate(
    std::unique_ptr<SampledData> sample,
    SamplingCallback callback,
    const power_manager::PowerSupplyProperties& prop) {
  if (prop.has_battery_discharge_rate()) {
    int discharge_rate_mW =
        static_cast<int>(prop.battery_discharge_rate() * 1000);
    for (auto it = sample->battery_samples.begin();
         it != sample->battery_samples.end(); it++) {
      it->second.set_discharge_rate(discharge_rate_mW);
    }
  }

  if (prop.has_battery_percent() && prop.battery_percent() >= 0) {
    int percent = static_cast<int>(prop.battery_percent());
    for (auto it = sample->battery_samples.begin();
         it != sample->battery_samples.end(); it++) {
      it->second.set_charge_rate(percent);
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InvokeCpuTempFetcher, cpu_temp_fetcher_),
      base::BindOnce(&DeviceStatusCollector::ReceiveCPUTemperature,
                     weak_factory_.GetWeakPtr(), std::move(sample),
                     std::move(callback)));
}

void DeviceStatusCollector::ReceiveCPUTemperature(
    std::unique_ptr<SampledData> sample,
    SamplingCallback callback,
    std::vector<em::CPUTempInfo> measurements) {
  auto timestamp = sample->timestamp.ToJavaTime();
  for (const auto& measurement : measurements) {
    sample->cpu_samples[measurement.cpu_label()] = measurement;
    sample->cpu_samples[measurement.cpu_label()].set_timestamp(timestamp);
  }
  AddDataSample(std::move(sample), std::move(callback));
}

void DeviceStatusCollector::AddDataSample(std::unique_ptr<SampledData> sample,
                                          SamplingCallback callback) {
  sampled_data_.push_back(std::move(sample));

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (sampled_data_.size() > kMaxResourceUsageSamples)
    sampled_data_.pop_front();
  // We have two code paths that end here. One is regular sampling, that does
  // not have final callback, and full report request, that would use callback
  // to receive ProbeResponse.
  if (!callback.is_null())
    std::move(callback).Run();
}

void DeviceStatusCollector::FetchCrosHealthdData(
    CrosHealthdCollectionMode mode,
    CrosHealthdDataReceiver callback) {
  using chromeos::cros_healthd::mojom::ProbeCategoryEnum;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<ProbeCategoryEnum> categories_to_probe;
  SamplingProbeResultCallback completion_callback;
  switch (mode) {
    case CrosHealthdCollectionMode::kFull: {
      if (report_vpd_info_ || report_system_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kSystem);
      if (report_storage_status_) {
        categories_to_probe.push_back(
            ProbeCategoryEnum::kNonRemovableBlockDevices);
      }
      if (report_power_status_)
        categories_to_probe.push_back(ProbeCategoryEnum::kBattery);
      if (report_cpu_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kCpu);
      if (report_timezone_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kTimezone);
      if (report_memory_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kMemory);
      if (report_backlight_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kBacklight);
      if (report_fan_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kFan);
      if (report_bluetooth_info_)
        categories_to_probe.push_back(ProbeCategoryEnum::kBluetooth);

      completion_callback =
          base::BindOnce(&DeviceStatusCollector::OnProbeDataFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback));
      break;
    }
    case CrosHealthdCollectionMode::kBattery: {
      categories_to_probe.push_back(ProbeCategoryEnum::kBattery);
      break;
    }
  }

  auto sample = std::make_unique<SampledData>();
  sample->timestamp = base::Time::Now();

  chromeos::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      categories_to_probe,
      base::BindOnce(&DeviceStatusCollector::SampleProbeData,
                     weak_factory_.GetWeakPtr(), std::move(sample),
                     std::move(completion_callback)));
}

void DeviceStatusCollector::OnProbeDataFetched(
    CrosHealthdDataReceiver callback,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(std::move(reply), sampled_data_);
}

bool DeviceStatusCollector::ShouldFetchCrosHealthdData() const {
  return report_vpd_info_ || report_power_status_ || report_storage_status_ ||
         report_cpu_info_ || report_timezone_info_ || report_memory_info_ ||
         report_backlight_info_ || report_fan_info_ || report_bluetooth_info_ ||
         report_system_info_;
}

void DeviceStatusCollector::ReportingUsersChanged() {
  std::vector<std::string> reporting_users;
  for (auto& value :
       pref_service_->GetList(prefs::kReportingUsers)->GetList()) {
    if (value.is_string())
      reporting_users.push_back(value.GetString());
  }

  activity_storage_->FilterActivityPeriodsByUsers(reporting_users);
}

std::string DeviceStatusCollector::GetUserForActivityReporting() const {
  // Primary user is used as unique identifier of a single session, even for
  // multi-user sessions.
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user || !primary_user->HasGaiaAccount())
    return std::string();

  // Report only affiliated users for enterprise reporting.
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();
  if (!ash::ChromeUserManager::Get()->ShouldReportUser(primary_user_email)) {
    return std::string();
  }
  return primary_user_email;
}

bool DeviceStatusCollector::IncludeEmailsInActivityReports() const {
  // Including the users' email addresses in enterprise reporting depends on the
  // |kReportDeviceUsers| preference.
  return report_users_;
}

bool DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* status) {
  // If user reporting is off, data should be aggregated per day.
  // Signed-in user is reported in non-enterprise reporting.
  activity_storage_->RemoveOverlappingActivityPeriods();
  auto activity_times = activity_storage_->GetFilteredActivityPeriods(
      !IncludeEmailsInActivityReports());

  bool anything_reported = false;
  for (const auto& activity_pair : activity_times) {
    const auto& user_email = activity_pair.first;
    const auto& activity_periods = activity_pair.second;

    for (const auto& activity_period : activity_periods) {
      // This is correct even when there are leap seconds, because when a leap
      // second occurs, two consecutive seconds have the same timestamp.
      int64_t end_timestamp =
          activity_period.start_timestamp() + Time::kMillisecondsPerDay;

      em::ActiveTimePeriod* active_period = status->add_active_periods();
      em::TimePeriod* period = active_period->mutable_time_period();
      period->set_start_timestamp(activity_period.start_timestamp());
      period->set_end_timestamp(end_timestamp);
      active_period->set_active_duration(activity_period.end_timestamp() -
                                         activity_period.start_timestamp());
      // Report user email only if users reporting is turned on.
      if (!user_email.empty()) {
        active_period->set_user_email(user_email);
      }
      if (last_reported_end_timestamp_ < end_timestamp) {
        last_reported_end_timestamp_ = end_timestamp;
      }
      anything_reported = true;
    }
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* status) {
  status->set_os_version(os_version_);
  status->set_browser_version(version_info::GetVersionNumber());
  status->set_channel(ConvertToProtoChannel(chrome::GetChannel()));

  // TODO(b/144081278): Remove when resolved.
  // When firmware version is not fetched, report error instead.
  status->set_firmware_version(
      !firmware_version_.empty() ? firmware_version_ : firmware_fetch_error_);

  em::TpmVersionInfo* const tpm_version_info =
      status->mutable_tpm_version_info();
  tpm_version_info->set_family(tpm_version_reply_.family());
  tpm_version_info->set_spec_level(tpm_version_reply_.spec_level());
  tpm_version_info->set_manufacturer(tpm_version_reply_.manufacturer());
  tpm_version_info->set_tpm_model(tpm_version_reply_.tpm_model());
  tpm_version_info->set_firmware_version(tpm_version_reply_.firmware_version());
  tpm_version_info->set_vendor_specific(tpm_version_reply_.vendor_specific());

  return true;
}

bool DeviceStatusCollector::GetWriteProtectSwitch(
    em::DeviceStatusReportRequest* status) {
  std::string firmware_write_protect;
  if (!statistics_provider_->GetMachineStatistic(
          chromeos::system::kFirmwareWriteProtectCurrentKey,
          &firmware_write_protect)) {
    // TODO(crbug.com/1123153): Remove logging after the bug is fixed.
    LOG(WARNING) << "Missing "
                 << chromeos::system::kFirmwareWriteProtectCurrentKey
                 << " statistics";
    return false;
  }
  // TODO(crbug.com/1123153): Remove logging after the bug is fixed.
  LOG(WARNING) << "Statistics "
               << chromeos::system::kFirmwareWriteProtectCurrentKey << ": "
               << firmware_write_protect;

  if (firmware_write_protect ==
      chromeos::system::kFirmwareWriteProtectCurrentValueOff) {
    status->set_write_protect_switch(false);
  } else if (firmware_write_protect ==
             chromeos::system::kFirmwareWriteProtectCurrentValueOn) {
    status->set_write_protect_switch(true);
  } else {
    return false;
  }
  return true;
}

bool DeviceStatusCollector::GetNetworkInterfaces(
    em::DeviceStatusReportRequest* status) {
  // Maps shill device type strings to proto enum constants.
  static const struct {
    const char* type_string;
    em::NetworkInterface::NetworkDeviceType type_constant;
  } kDeviceTypeMap[] = {
      {
          shill::kTypeEthernet,
          em::NetworkInterface::TYPE_ETHERNET,
      },
      {
          shill::kTypeWifi,
          em::NetworkInterface::TYPE_WIFI,
      },
      {
          shill::kTypeCellular,
          em::NetworkInterface::TYPE_CELLULAR,
      },
  };

  // Maps shill device connection status to proto enum constants.
  static const struct {
    const char* state_string;
    em::NetworkState::ConnectionState state_constant;
  } kConnectionStateMap[] = {
      {shill::kStateIdle, em::NetworkState::IDLE},
      {shill::kStateCarrier, em::NetworkState::CARRIER},
      {shill::kStateAssociation, em::NetworkState::ASSOCIATION},
      {shill::kStateConfiguration, em::NetworkState::CONFIGURATION},
      {shill::kStateReady, em::NetworkState::READY},
      {shill::kStatePortal, em::NetworkState::PORTAL},
      {shill::kStateNoConnectivity, em::NetworkState::PORTAL},
      {shill::kStateRedirectFound, em::NetworkState::PORTAL},
      {shill::kStatePortalSuspected, em::NetworkState::PORTAL},
      {shill::kStateOffline, em::NetworkState::OFFLINE},
      {shill::kStateOnline, em::NetworkState::ONLINE},
      {shill::kStateDisconnect, em::NetworkState::DISCONNECT},
      {shill::kStateFailure, em::NetworkState::FAILURE},
      {shill::kStateActivationFailure, em::NetworkState::ACTIVATION_FAILURE},
  };

  chromeos::NetworkStateHandler::DeviceStateList device_list;
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetDeviceList(&device_list);

  bool anything_reported = false;
  chromeos::NetworkStateHandler::DeviceStateList::const_iterator device;
  for (device = device_list.begin(); device != device_list.end(); ++device) {
    // Determine the type enum constant for |device|.
    size_t type_idx = 0;
    for (; type_idx < base::size(kDeviceTypeMap); ++type_idx) {
      if ((*device)->type() == kDeviceTypeMap[type_idx].type_string)
        break;
    }

    // If the type isn't in |kDeviceTypeMap|, the interface is not relevant for
    // reporting. This filters out VPN devices.
    if (type_idx >= base::size(kDeviceTypeMap))
      continue;

    em::NetworkInterface* interface = status->add_network_interfaces();
    interface->set_type(kDeviceTypeMap[type_idx].type_constant);
    if (!(*device)->mac_address().empty())
      interface->set_mac_address((*device)->mac_address());
    if (!(*device)->meid().empty())
      interface->set_meid((*device)->meid());
    if (!(*device)->imei().empty())
      interface->set_imei((*device)->imei());
    if (!(*device)->path().empty())
      interface->set_device_path((*device)->path());
    anything_reported = true;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();
  // Don't write network state for unaffiliated users or when no user is signed
  // in.
  if (!primary_user || !primary_user->IsAffiliated()) {
    return anything_reported;
  }

  // Walk the various networks and store their state in the status report.
  chromeos::NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(
      chromeos::NetworkTypePattern::Default(),
      true,   // configured_only
      false,  // visible_only
      0,      // no limit to number of results
      &state_list);

  for (const chromeos::NetworkState* state : state_list) {
    // Determine the connection state and signal strength for |state|.
    em::NetworkState::ConnectionState connection_state_enum =
        em::NetworkState::UNKNOWN;
    const std::string connection_state_string(state->connection_state());
    for (size_t i = 0; i < base::size(kConnectionStateMap); ++i) {
      if (connection_state_string == kConnectionStateMap[i].state_string) {
        connection_state_enum = kConnectionStateMap[i].state_constant;
        break;
      }
    }

    // Copy fields from NetworkState into the status report.
    em::NetworkState* proto_state = status->add_network_states();
    proto_state->set_connection_state(connection_state_enum);
    anything_reported = true;

    // Report signal strength for wifi connections.
    if (state->type() == shill::kTypeWifi) {
      // If shill has provided a signal strength, convert it to dBm and store it
      // in the status report. A signal_strength() of 0 connotes "no signal"
      // rather than "really weak signal", so we only report signal strength if
      // it is non-zero.
      if (state->signal_strength()) {
        proto_state->set_signal_strength(
            ConvertWifiSignalStrength(state->signal_strength()));
      }
    }

    if (!state->device_path().empty())
      proto_state->set_device_path(state->device_path());

    std::string ip_address = state->GetIpAddress();
    if (!ip_address.empty())
      proto_state->set_ip_address(ip_address);

    std::string gateway = state->GetGateway();
    if (!gateway.empty())
      proto_state->set_gateway(gateway);
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetUsers(em::DeviceStatusReportRequest* status) {
  const user_manager::UserList& users =
      ash::ChromeUserManager::Get()->GetUsers();

  bool anything_reported = false;
  for (auto* user : users) {
    // Only users with gaia accounts (regular) are reported.
    if (!user->HasGaiaAccount())
      continue;

    em::DeviceUser* device_user = status->add_users();
    if (ash::ChromeUserManager::Get()->ShouldReportUser(
            user->GetAccountId().GetUserEmail())) {
      device_user->set_type(em::DeviceUser::USER_TYPE_MANAGED);
      device_user->set_email(user->GetAccountId().GetUserEmail());
    } else {
      device_user->set_type(em::DeviceUser::USER_TYPE_UNMANAGED);
      // Do not report the email address of unmanaged users.
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetHardwareStatus(
    scoped_refptr<DeviceStatusCollectorState> state) {
  em::DeviceStatusReportRequest* status =
      state->response_params().device_status.get();

  // Sample disk volume info in a background thread.
  state->SampleVolumeInfo(volume_info_fetcher_);

  // Add CPU utilization and free RAM. Note that these stats are sampled in
  // regular intervals. Unlike CPU temp and volume info these are not one-time
  // sampled values, hence the difference in logic.
  status->set_system_ram_total(base::SysInfo::AmountOfPhysicalMemory());
  status->clear_cpu_utilization_infos();
  status->clear_system_ram_free_infos();

  // TODO(anqing): remove these two cleanup operations after fields
  // 'system_ram_free_samples' and 'cpu_utilization_pct_samples' are deprecated.
  status->clear_system_ram_free_samples();
  status->clear_cpu_utilization_pct_samples();

  for (const ResourceUsage& usage : resource_usage_) {
    const int64_t usage_timestamp = usage.timestamp.ToJavaTime();

    em::CpuUtilizationInfo* cpu_utilization_info =
        status->add_cpu_utilization_infos();
    cpu_utilization_info->set_cpu_utilization_pct(usage.cpu_usage_percent);
    cpu_utilization_info->set_timestamp(usage_timestamp);

    em::SystemFreeRamInfo* system_ram_free_info =
        status->add_system_ram_free_infos();
    system_ram_free_info->set_size_in_bytes(usage.bytes_of_ram_free);
    system_ram_free_info->set_timestamp(usage_timestamp);

    // TODO(anqing): remove these two assignment operations after fields
    // 'system_ram_free_samples' and 'cpu_utilization_pct_samples' are
    // deprecated.
    status->add_cpu_utilization_pct_samples(usage.cpu_usage_percent);
    status->add_system_ram_free_samples(usage.bytes_of_ram_free);
  }

  // Get the current device sound volume level.
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  status->set_sound_volume(audio_handler->GetOutputVolumePercent());

  // Fetch TPM status information on a background thread.
  state->FetchTpmStatus(tpm_status_fetcher_);

  // clear
  status->clear_cpu_temp_infos();

  if (report_storage_status_)
    state->FetchEMMCLifeTime(emmc_lifetime_fetcher_);

  if (ShouldFetchCrosHealthdData()) {
    state->FetchCrosHealthdData(cros_healthd_data_fetcher_, report_system_info_,
                                report_vpd_info_);
  } else {
    // Sample CPU temperature in a background thread.
    state->SampleCPUTempInfo(cpu_temp_fetcher_);
  }

  // Fetch Stateful Partition Information on a background thread.
  state->FetchStatefulPartitionInfo(stateful_partition_info_fetcher_);

  return true;
}

bool DeviceStatusCollector::GetOsUpdateStatus(
    em::DeviceStatusReportRequest* status) {
  const base::Version platform_version(GetPlatformVersion());
  if (!platform_version.IsValid())
    return false;

  const std::string required_platform_version_string =
      ash::KioskAppManager::Get()->GetAutoLaunchAppRequiredPlatformVersion();
  em::OsUpdateStatus* os_update_status = status->mutable_os_update_status();

  const update_engine::StatusResult update_engine_status =
      chromeos::DBusThreadManager::Get()
          ->GetUpdateEngineClient()
          ->GetLastStatus();

  base::Optional<base::Version> required_platform_version;

  if (required_platform_version_string.empty()) {
    // If this is non-Kiosk session, the OS is considered as up-to-date if the
    // status of UpdateEngineClient is idle.
    if (update_engine_status.current_operation() ==
        update_engine::Operation::IDLE)
      required_platform_version = base::Version(platform_version);
  } else {
    // If this is Kiosk session, |required_platform_version| can be searched
    // from the KioskAppClient instance.
    required_platform_version = base::Version(required_platform_version_string);
    os_update_status->set_new_required_platform_version(
        required_platform_version->GetString());
  }

  // Get last reboot timestamp.
  const base::Time last_reboot_timestamp =
      base::Time::Now() - base::SysInfo::Uptime();

  os_update_status->set_last_reboot_timestamp(
      last_reboot_timestamp.ToJavaTime());

  // Get last check timestamp.
  // As the timestamp precision return from UpdateEngine is in seconds (see
  // time_t). It should be converted to milliseconds before being reported.
  const base::Time last_checked_timestamp =
      base::Time::FromTimeT(update_engine_status.last_checked_time());

  os_update_status->set_last_checked_timestamp(
      last_checked_timestamp.ToJavaTime());

  if (required_platform_version &&
      platform_version == *required_platform_version) {
    os_update_status->set_update_status(em::OsUpdateStatus::OS_UP_TO_DATE);
    return true;
  }

  if (update_engine_status.current_operation() ==
          update_engine::Operation::DOWNLOADING ||
      update_engine_status.current_operation() ==
          update_engine::Operation::VERIFYING ||
      update_engine_status.current_operation() ==
          update_engine::Operation::FINALIZING) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS);
    os_update_status->set_new_platform_version(
        update_engine_status.new_version());
  } else if (update_engine_status.current_operation() ==
             update_engine::Operation::UPDATED_NEED_REBOOT) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT);
    // Note the new_version could be a dummy "0.0.0.0" for some edge cases,
    // e.g. update engine is somehow restarted without a reboot.
    os_update_status->set_new_platform_version(
        update_engine_status.new_version());
  } else {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED);
  }

  return true;
}

bool DeviceStatusCollector::GetRunningKioskApp(
    em::DeviceStatusReportRequest* status) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  // Only generate running kiosk app reports if we are in an auto-launched kiosk
  // session.
  if (!account)
    return false;

  em::AppStatus* running_kiosk_app = status->mutable_running_kiosk_app();
  if (account->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    running_kiosk_app->set_app_id(account->kiosk_app_id);

    const std::string app_version = GetAppVersion(account->kiosk_app_id);
    if (app_version.empty()) {
      DLOG(ERROR) << "Unable to get version for extension: "
                  << account->kiosk_app_id;
    } else {
      running_kiosk_app->set_extension_version(app_version);
    }

    ash::KioskAppManager::App app_info;
    if (ash::KioskAppManager::Get()->GetApp(account->kiosk_app_id, &app_info)) {
      running_kiosk_app->set_required_platform_version(
          app_info.required_platform_version);
    }
  } else if (account->type == policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    // Use package name as app ID for ARC Kiosks.
    running_kiosk_app->set_app_id(account->arc_kiosk_app_info.package_name());
  } else if (account->type == policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
    running_kiosk_app->set_app_id(account->web_kiosk_app_info.url());
  } else {
    NOTREACHED();
  }
  return true;
}

bool DeviceStatusCollector::GetGraphicsStatus(
    scoped_refptr<DeviceStatusCollectorState> state) {
  // Fetch Graphics status on a background thread.
  state->FetchGraphicsStatus(graphics_status_fetcher_);

  return true;
}

bool DeviceStatusCollector::GetCrashReportInfo(
    scoped_refptr<DeviceStatusCollectorState> state) {
  state->FetchCrashReportInfo(crash_report_info_fetcher_);

  return true;
}

void DeviceStatusCollector::GetStatusAsync(StatusCollectorCallback response) {
  last_requested_ = clock_->Now();

  app_info_generator_.OnWillReport();

  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());
  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<DeviceStatusCollectorState> state(
      new DeviceStatusCollectorState(task_runner_, std::move(response)));
  // Gather device status (might queue some async queries)
  GetDeviceStatus(state);

  // Gather session status (might queue some async queries)
  GetSessionStatus(state);

  // If there are no outstanding async queries, e.g. from GetHardwareStatus(),
  // the destructor of |state| calls |response|. If there are async queries, the
  // queries hold references to |state|, so that |state| is only destroyed when
  // the last async query has finished.
}

void DeviceStatusCollector::GetDeviceStatus(
    scoped_refptr<DeviceStatusCollectorState> state) {
  em::DeviceStatusReportRequest* status =
      state->response_params().device_status.get();
  bool anything_reported = false;

  if (report_activity_times_)
    anything_reported |= GetActivityTimes(status);

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  if (report_boot_mode_) {
    base::Optional<std::string> boot_mode =
        StatusCollector::GetBootMode(statistics_provider_);
    if (boot_mode) {
      status->set_boot_mode(*boot_mode);
      anything_reported = true;
    }
  }

  if (report_network_interfaces_)
    anything_reported |= GetNetworkInterfaces(status);

  if (report_users_)
    anything_reported |= GetUsers(status);

  if (report_hardware_status_) {
    anything_reported |= GetHardwareStatus(state);
    anything_reported |= GetWriteProtectSwitch(status);
  }

  if (report_os_update_status_)
    anything_reported |= GetOsUpdateStatus(status);

  if (report_running_kiosk_app_)
    anything_reported |= GetRunningKioskApp(status);

  if (report_graphics_status_)
    anything_reported |= GetGraphicsStatus(state);

  if (report_crash_report_info_ && stat_reporting_pref_)
    anything_reported |= GetCrashReportInfo(state);

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->response_params().device_status.reset();
}

bool DeviceStatusCollector::GetSessionStatusForUser(
    scoped_refptr<DeviceStatusCollectorState> state,
    em::SessionStatusReportRequest* status,
    const user_manager::User* user) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;

  bool anything_reported_user = false;

  const bool report_android_status =
      profile->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);
  if (report_android_status)
    anything_reported_user |= GetAndroidStatus(status, state);

  const bool report_crostini_usage = profile->GetPrefs()->GetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled);
  if (report_crostini_usage)
    anything_reported_user |= GetCrostiniUsage(status, profile);

  if (anything_reported_user && !user->IsDeviceLocalAccount())
    status->set_user_dm_token(GetDMTokenForProfile(profile));

  // Time zone is not reported in enterprise reports.

  return anything_reported_user;
}

void DeviceStatusCollector::GetSessionStatus(
    scoped_refptr<DeviceStatusCollectorState> state) {
  em::SessionStatusReportRequest* status =
      state->response_params().session_status.get();
  bool anything_reported = false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();

  if (report_kiosk_session_status_)
    anything_reported |= GetKioskSessionStatus(status);

  // Only report affiliated users' data in enterprise reporting. Note that
  // device-local accounts are also affiliated. Currently we only report for the
  // primary user.
  if (primary_user && primary_user->IsAffiliated()) {
    anything_reported |= GetSessionStatusForUser(state, status, primary_user);
  }

  // |app_infos|
  const auto app_infos = app_info_generator_.Generate();
  anything_reported |= app_infos.has_value();
  if (app_infos) {
    *status->mutable_app_infos() = {app_infos.value().begin(),
                                    app_infos.value().end()};
  }

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->response_params().session_status.reset();
}

bool DeviceStatusCollector::GetKioskSessionStatus(
    em::SessionStatusReportRequest* status) {
  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  if (!account)
    return false;

  // Get the account ID associated with this user.
  status->set_device_local_account_id(account->account_id);
  em::AppStatus* app_status = status->add_installed_apps();
  if (account->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    app_status->set_app_id(account->kiosk_app_id);

    // Look up the app and get the version.
    const std::string app_version = GetAppVersion(account->kiosk_app_id);
    if (app_version.empty()) {
      DLOG(ERROR) << "Unable to get version for extension: "
                  << account->kiosk_app_id;
    } else {
      app_status->set_extension_version(app_version);
    }
  } else if (account->type == policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    // Use package name as app ID for ARC Kiosks.
    app_status->set_app_id(account->arc_kiosk_app_info.package_name());
  } else if (account->type == policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
    app_status->set_app_id(account->web_kiosk_app_info.url());
  } else {
    NOTREACHED();
  }

  return true;
}

bool DeviceStatusCollector::GetAndroidStatus(
    em::SessionStatusReportRequest* status,
    const scoped_refptr<DeviceStatusCollectorState>& state) {
  return state->FetchAndroidStatus(android_status_fetcher_);
}

bool DeviceStatusCollector::GetCrostiniUsage(
    em::SessionStatusReportRequest* status,
    Profile* profile) {
  if (!profile->GetPrefs()->HasPrefPath(
          crostini::prefs::kCrostiniLastLaunchTimeWindowStart)) {
    return false;
  }

  em::CrostiniStatus* const crostini_status = status->mutable_crostini_status();
  const int64_t last_launch_time_window_start = profile->GetPrefs()->GetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart);
  const std::string& termina_version = profile->GetPrefs()->GetString(
      crostini::prefs::kCrostiniLastLaunchTerminaComponentVersion);
  crostini_status->set_last_launch_time_window_start_timestamp(
      last_launch_time_window_start);
  crostini_status->set_last_launch_vm_image_version(termina_version);

  if (profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled) &&
      base::FeatureList::IsEnabled(
          features::kCrostiniAdditionalEnterpriseReporting)) {
    const std::string& vm_kernel_version = profile->GetPrefs()->GetString(
        crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion);
    crostini_status->set_last_launch_vm_kernel_version(vm_kernel_version);

    AddCrostiniAppListForProfile(profile, crostini_status);
  }

  return true;
}

std::string DeviceStatusCollector::GetAppVersion(
    const std::string& kiosk_app_id) {
  Profile* const profile = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
  const extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* const extension = registry->GetExtensionById(
      kiosk_app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return std::string();
  return extension->VersionString();
}

// TODO(crbug.com/827386): move public API methods above private ones after
// common methods are extracted.
void DeviceStatusCollector::OnSubmittedSuccessfully() {
  activity_storage_->TrimActivityPeriods(last_reported_end_timestamp_,
                                         std::numeric_limits<int64_t>::max());
  app_info_generator_.OnReportedSuccessfully(last_requested_);
}

bool DeviceStatusCollector::ShouldReportActivityTimes() const {
  return report_activity_times_;
}
bool DeviceStatusCollector::ShouldReportNetworkInterfaces() const {
  return report_network_interfaces_;
}
bool DeviceStatusCollector::ShouldReportUsers() const {
  return report_users_;
}
bool DeviceStatusCollector::ShouldReportHardwareStatus() const {
  return report_hardware_status_;
}
bool DeviceStatusCollector::ShouldReportCrashReportInfo() const {
  return report_crash_report_info_ && stat_reporting_pref_;
}
bool DeviceStatusCollector::ShouldReportAppInfoAndActivity() const {
  return report_app_info_;
}

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(
    std::pair<const std::string&, const std::string&> version) {
  firmware_version_ = version.first;
  firmware_fetch_error_ = version.second;
}

void DeviceStatusCollector::OnGetTpmVersion(
    const ::tpm_manager::GetVersionInfoReply& reply) {
  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << "Failed to get tpm version; status: " << reply.status();
  }
  tpm_version_reply_ = reply;
}

}  // namespace policy
