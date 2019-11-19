// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_reporting_util.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/status_collector/enterprise_activity_storage.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector_state.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
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
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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
                               std::vector<em::CPUTempInfo>& out_contents) {
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
      out_contents.push_back(info);
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
      if (!ReadTemperatureSensorInfo(device_path, contents)) {
        ReadTemperatureSensorInfo(hwmon_path, contents);
      }
    } else {
      ReadTemperatureSensorInfo(hwmon_path, contents);
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

bool ReadAndroidStatus(
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver) {
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
  instance->GetStatus(receiver);
  return true;
}

// Converts the given GetTpmStatusReply to TpmStatusInfo.
policy::TpmStatusInfo GetTpmStatusReplyToTpmStatusInfo(
    const base::Optional<cryptohome::BaseReply>& reply) {
  policy::TpmStatusInfo tpm_status_info;

  if (!reply.has_value()) {
    LOG(ERROR) << "GetTpmStatus call failed with empty reply.";
    return tpm_status_info;
  }
  if (reply->has_error() &&
      reply->error() != cryptohome::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "GetTpmStatus failed with error: " << reply->error();
    return tpm_status_info;
  }
  if (!reply->HasExtension(cryptohome::GetTpmStatusReply::reply)) {
    LOG(ERROR)
        << "GetTpmStatus failed with no GetTpmStatusReply extension in reply.";
    return tpm_status_info;
  }

  auto reply_proto = reply->GetExtension(cryptohome::GetTpmStatusReply::reply);

  tpm_status_info.enabled = reply_proto.enabled();
  tpm_status_info.owned = reply_proto.owned();
  tpm_status_info.initialized = reply_proto.initialized();
  tpm_status_info.attestation_prepared = reply_proto.attestation_prepared();
  tpm_status_info.attestation_enrolled = reply_proto.attestation_enrolled();
  tpm_status_info.dictionary_attack_counter =
      reply_proto.dictionary_attack_counter();
  tpm_status_info.dictionary_attack_threshold =
      reply_proto.dictionary_attack_threshold();
  tpm_status_info.dictionary_attack_lockout_in_effect =
      reply_proto.dictionary_attack_lockout_in_effect();
  tpm_status_info.dictionary_attack_lockout_seconds_remaining =
      reply_proto.dictionary_attack_lockout_seconds_remaining();
  tpm_status_info.boot_lockbox_finalized = reply_proto.boot_lockbox_finalized();

  return tpm_status_info;
}

void ReadTpmStatus(policy::DeviceStatusCollector::TpmStatusReceiver callback) {
  // D-Bus calls are allowed only on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::CryptohomeClient::Get()->GetTpmStatus(
      cryptohome::GetTpmStatusRequest(),
      base::BindOnce(
          [](policy::DeviceStatusCollector::TpmStatusReceiver callback,
             base::Optional<cryptohome::BaseReply> reply) {
            std::move(callback).Run(GetTpmStatusReplyToTpmStatusInfo(reply));
          },
          std::move(callback)));
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
  auto user_type = chromeos::LoginState::Get()->GetLoggedInUserType();
  return user_type == chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP ||
         user_type == chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP;
}

// Utility method to turn cpu_temp_fetcher_ to OnceCallback
std::vector<em::CPUTempInfo> InvokeCpuTempFetcher(
    policy::DeviceStatusCollector::CPUTempFetcher fetcher) {
  return fetcher.Run();
}

// Utility method to complete information for a reported Crostini App.
// Returns whether all required App information could be retrieved or not.
bool AddCrostiniAppInfo(
    const crostini::CrostiniRegistryService::Registration& registration,
    em::CrostiniApp* const app) {
  app->set_app_name(registration.Name());
  const base::Time last_launch_time = registration.LastLaunchTime();
  if (!last_launch_time.is_null()) {
    app->set_last_launch_time_window_start_timestamp(
        crostini::GetThreeDayWindowStart(last_launch_time).ToJavaTime());
  }

  if (registration.is_terminal_app()) {
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
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  for (const auto& pair : registry_service->GetRegisteredApps()) {
    const std::string& registered_app_id = pair.first;
    const auto& registration = pair.second;
    em::CrostiniApp* const app = crostini_status->add_installed_apps();
    if (!AddCrostiniAppInfo(registration, app)) {
      LOG(ERROR) << "Could not retrieve all required information for "
                    "registered app_id: "
                 << registered_app_id;
    }
  }
}

}  // namespace

namespace policy {

class DeviceStatusCollectorState : public StatusCollectorState {
 public:
  explicit DeviceStatusCollectorState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      const StatusCollectorCallback& response)
      : StatusCollectorState(task_runner, response) {}

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
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(volume_info_fetcher, mount_points),
        base::Bind(&DeviceStatusCollectorState::OnVolumeInfoReceived, this));
  }

  // Queues an async callback to query CPU temperature information.
  void SampleCPUTempInfo(
      const DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher) {
    // Call out to the blocking pool to sample CPU temp.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        cpu_temp_fetcher,
        base::Bind(&DeviceStatusCollectorState::OnCPUTempInfoReceived, this));
  }

  bool FetchAndroidStatus(const DeviceStatusCollector::AndroidStatusFetcher&
                              android_status_fetcher) {
    return android_status_fetcher.Run(
        base::Bind(&DeviceStatusCollectorState::OnAndroidInfoReceived, this));
  }

  void FetchTpmStatus(
      const DeviceStatusCollector::TpmStatusFetcher& tpm_status_fetcher) {
    tpm_status_fetcher.Run(
        base::BindOnce(&DeviceStatusCollectorState::OnTpmStatusReceived, this));
  }

  void FetchCrosHealthdData(
      const policy::DeviceStatusCollector::CrosHealthdDataFetcher&
          cros_healthd_data_fetcher) {
    cros_healthd_data_fetcher.Run(base::BindOnce(
        &DeviceStatusCollectorState::OnCrosHealthdDataReceived, this));
  }

  void FetchEMMCLifeTime(
      const policy::DeviceStatusCollector::EMMCLifetimeFetcher&
          emmc_lifetime_fetcher) {
    // Call out to the blocking pool to read disklifetimeestimation.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(emmc_lifetime_fetcher),
        base::BindOnce(&DeviceStatusCollectorState::OnEMMCLifetimeReceived,
                       this));
  }

  void FetchStatefulPartitionInfo(
      const policy::DeviceStatusCollector::StatefulPartitionInfoFetcher&
          stateful_partition_info_fetcher) {
    // Call out to the blocking pool to read stateful partition information.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(stateful_partition_info_fetcher),
        base::BindOnce(
            &DeviceStatusCollectorState::OnStatefulPartitionInfoReceived,
            this));
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
    DCHECK(response_params_.device_status->cpu_temp_infos_size() == 0);

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
      chromeos::cros_healthd::mojom::TelemetryInfoPtr probe_result,
      const base::circular_deque<std::unique_ptr<SampledData>>& samples) {
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

    const auto& block_device_info = probe_result->block_device_info;
    if (block_device_info) {
      em::StorageStatus* const storage_status_out =
          response_params_.device_status->mutable_storage_status();
      for (const auto& storage : block_device_info.value()) {
        em::DiskInfo* const disk_info_out = storage_status_out->add_disks();
        disk_info_out->set_serial(base::NumberToString(storage->serial));
        disk_info_out->set_manufacturer(
            base::NumberToString(storage->manufacturer_id));
        disk_info_out->set_model(storage->name);
        disk_info_out->set_type(storage->type);
        disk_info_out->set_size(storage->size);
      }
    }
    const auto& vpd_info = probe_result->vpd_info;
    if (!vpd_info.is_null()) {
      em::SystemStatus* const system_status_out =
          response_params_.device_status->mutable_system_status();
      system_status_out->set_vpd_sku_number(vpd_info->sku_number);
    }
    const auto& battery_info = probe_result->battery_info;
    if (!battery_info.is_null()) {
      em::PowerStatus* const power_status_out =
          response_params_.device_status->mutable_power_status();
      em::BatteryInfo* const battery_info_out =
          power_status_out->add_batteries();
      battery_info_out->set_serial(battery_info->serial_number);
      battery_info_out->set_manufacturer(battery_info->vendor);
      battery_info_out->set_cycle_count(battery_info->cycle_count);
      // Convert Ah to mAh:
      battery_info_out->set_design_capacity(
          std::lround(battery_info->charge_full_design * 1000));
      battery_info_out->set_full_charge_capacity(
          std::lround(battery_info->charge_full * 1000));
      // Convert V to mV:
      battery_info_out->set_design_min_voltage(
          std::lround(battery_info->voltage_min_design * 1000));
      if (battery_info->manufacture_date_smart > 0) {
        // manufacture_date in (((year-1980) * 16 + month) * 32 + day) format.
        int remainder = battery_info->manufacture_date_smart;
        int day = remainder % 32;
        remainder /= 32;
        int month = remainder % 16;
        remainder /= 16;
        int year = remainder + 1980;
        // set manufacture_date in yyyy-mm-dd format.
        battery_info_out->set_manufacture_date(
            base::StringPrintf("%04d-%02d-%02d", year, month, day));
      }

      for (const std::unique_ptr<SampledData>& sample_data : samples) {
        auto it = sample_data->battery_samples.find(battery_info->model_name);
        if (it != sample_data->battery_samples.end())
          battery_info_out->add_samples()->CheckTypeAndMergeFrom(it->second);
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
    DCHECK(hdsi.available_space() >= 0);
    DCHECK(hdsi.total_space() >= hdsi.available_space());
    stateful_partition_info->CopyFrom(hdsi);
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
    const CrosHealthdDataFetcher& cros_healthd_data_fetcher)
    : StatusCollector(provider, chromeos::CrosSettings::Get()),
      pref_service_(pref_service),
      volume_info_fetcher_(volume_info_fetcher),
      cpu_statistics_fetcher_(cpu_statistics_fetcher),
      cpu_temp_fetcher_(cpu_temp_fetcher),
      android_status_fetcher_(android_status_fetcher),
      tpm_status_fetcher_(tpm_status_fetcher),
      emmc_lifetime_fetcher_(emmc_lifetime_fetcher),
      stateful_partition_info_fetcher_(stateful_partition_info_fetcher),
      cros_healthd_data_fetcher_(cros_healthd_data_fetcher),
      power_manager_(chromeos::PowerManagerClient::Get()) {
  // protected fields of `StatusCollector`.
  max_stored_past_activity_interval_ = kMaxStoredPastActivityInterval;
  max_stored_future_activity_interval_ = kMaxStoredFutureActivityInterval;

  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_ = base::SequencedTaskRunnerHandle::Get();

  if (volume_info_fetcher_.is_null())
    volume_info_fetcher_ = base::Bind(&GetVolumeInfo);

  if (cpu_statistics_fetcher_.is_null())
    cpu_statistics_fetcher_ = base::Bind(&ReadCPUStatistics);

  if (cpu_temp_fetcher_.is_null())
    cpu_temp_fetcher_ = base::Bind(&ReadCPUTempInfo);

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::Bind(&ReadAndroidStatus);

  if (tpm_status_fetcher_.is_null())
    tpm_status_fetcher_ = base::BindRepeating(&ReadTpmStatus);

  if (emmc_lifetime_fetcher_.is_null())
    emmc_lifetime_fetcher_ = base::BindRepeating(&ReadDiskLifeTimeEstimation);

  if (stateful_partition_info_fetcher_.is_null())
    stateful_partition_info_fetcher_ = base::Bind(&ReadStatefulPartitionInfo);

  if (cros_healthd_data_fetcher_.is_null()) {
    cros_healthd_data_fetcher_ =
        base::BindRepeating(&DeviceStatusCollector::FetchCrosHealthdData,
                            weak_factory_.GetWeakPtr());
  }

  idle_poll_timer_.Start(FROM_HERE,
                         TimeDelta::FromSeconds(kIdlePollIntervalSeconds), this,
                         &DeviceStatusCollector::CheckIdleState);
  resource_usage_sampling_timer_.Start(
      FROM_HERE, TimeDelta::FromSeconds(kResourceUsageSampleIntervalSeconds),
      this, &DeviceStatusCollector::SampleResourceUsage);

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  base::Closure callback = base::Bind(
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

  power_manager_->AddObserver(this);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS, firmware, and TPM version info.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&DeviceStatusCollector::OnOSVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&DeviceStatusCollector::OnOSFirmware,
                 weak_factory_.GetWeakPtr()));
  chromeos::tpm_util::GetTpmVersion(base::BindOnce(
      &DeviceStatusCollector::OnTpmVersion, weak_factory_.GetWeakPtr()));

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

DeviceStatusCollector::~DeviceStatusCollector() {
  power_manager_->RemoveObserver(this);
}

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
          base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

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

  Time now = GetCurrentTime();

  // For kiosk apps we report total uptime instead of active time.
  if (state == ui::IDLE_STATE_ACTIVE || IsKioskApp()) {
    std::string user_email = GetUserForActivityReporting();
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    int active_seconds = (now - last_idle_check_).InSeconds();
    if (active_seconds < 0 ||
        active_seconds >= static_cast<int>((2 * kIdlePollIntervalSeconds))) {
      activity_storage_->AddActivityPeriod(
          now - TimeDelta::FromSeconds(kIdlePollIntervalSeconds), now,
          user_email);
    } else {
      activity_storage_->AddActivityPeriod(last_idle_check_, now, user_email);
    }

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
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      cpu_statistics_fetcher_,
      base::Bind(&DeviceStatusCollector::ReceiveCPUStatistics,
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
    std::vector<chromeos::cros_healthd::mojom::ProbeCategoryEnum>
        categories_to_probe = {
            chromeos::cros_healthd::mojom::ProbeCategoryEnum::kBattery};
    chromeos::cros_healthd::ServiceConnection::GetInstance()
        ->ProbeTelemetryInfo(
            categories_to_probe,
            base::BindOnce(&DeviceStatusCollector::SampleProbeData,
                           weak_factory_.GetWeakPtr(), std::move(sample),
                           SamplingProbeResultCallback()));
  } else {
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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

  const auto& battery = result->battery_info;
  if (!battery.is_null()) {
    enterprise_management::BatterySample battery_sample;
    battery_sample.set_timestamp(sample->timestamp.ToJavaTime());
    // Convert V to mV:
    battery_sample.set_voltage(std::lround(battery->voltage_now * 1000));
    // Convert Ah to mAh:
    battery_sample.set_remaining_capacity(
        std::lround(battery->charge_now * 1000));
    // Convert 0.1 Kelvin to Celsius:
    battery_sample.set_temperature((battery->temperature_smart - 2731) / 10);
    sample->battery_samples[battery->model_name] = battery_sample;
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

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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
    CrosHealthdDataReceiver callback) {
  using chromeos::cros_healthd::mojom::ProbeCategoryEnum;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<ProbeCategoryEnum> categories_to_probe = {
      ProbeCategoryEnum::kCachedVpdData};
  if (report_storage_status_)
    categories_to_probe.push_back(ProbeCategoryEnum::kNonRemovableBlockDevices);
  if (report_power_status_)
    categories_to_probe.push_back(ProbeCategoryEnum::kBattery);

  auto sample = std::make_unique<SampledData>();
  sample->timestamp = base::Time::Now();
  auto completion_callback =
      base::BindOnce(&DeviceStatusCollector::OnProbeDataFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback));

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
  if (!chromeos::ChromeUserManager::Get()->ShouldReportUser(
          primary_user_email)) {
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
  std::vector<ActivityStorage::ActivityPeriod> activity_times =
      activity_storage_->GetFilteredActivityPeriods(
          !IncludeEmailsInActivityReports());

  bool anything_reported = false;
  for (const auto& activity_period : activity_times) {
    // This is correct even when there are leap seconds, because when a leap
    // second occurs, two consecutive seconds have the same timestamp.
    int64_t end_timestamp =
        activity_period.start_timestamp + Time::kMillisecondsPerDay;

    em::ActiveTimePeriod* active_period = status->add_active_periods();
    em::TimePeriod* period = active_period->mutable_time_period();
    period->set_start_timestamp(activity_period.start_timestamp);
    period->set_end_timestamp(end_timestamp);
    active_period->set_active_duration(activity_period.activity_milliseconds);
    // Report user email only if users reporting is turned on.
    if (!activity_period.user_email.empty())
      active_period->set_user_email(activity_period.user_email);
    if (activity_period.start_timestamp >= last_reported_day_) {
      last_reported_day_ = activity_period.start_timestamp;
      duration_for_last_reported_day_ = activity_period.activity_milliseconds;
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* status) {
  status->set_os_version(os_version_);
  status->set_browser_version(version_info::GetVersionNumber());
  status->set_channel(ConvertToProtoChannel(chrome::GetChannel()));
  status->set_firmware_version(firmware_version_);

  em::TpmVersionInfo* const tpm_version_info =
      status->mutable_tpm_version_info();
  tpm_version_info->set_family(tpm_version_info_.family);
  tpm_version_info->set_spec_level(tpm_version_info_.spec_level);
  tpm_version_info->set_manufacturer(tpm_version_info_.manufacturer);
  tpm_version_info->set_tpm_model(tpm_version_info_.tpm_model);
  tpm_version_info->set_firmware_version(tpm_version_info_.firmware_version);
  tpm_version_info->set_vendor_specific(tpm_version_info_.vendor_specific);

  return true;
}

bool DeviceStatusCollector::GetWriteProtectSwitch(
    em::DeviceStatusReportRequest* status) {
  std::string firmware_write_protect;
  if (!statistics_provider_->GetMachineStatistic(
          chromeos::system::kFirmwareWriteProtectBootKey,
          &firmware_write_protect)) {
    return false;
  }

  if (firmware_write_protect ==
      chromeos::system::kFirmwareWriteProtectBootValueOff) {
    status->set_write_protect_switch(false);
  } else if (firmware_write_protect ==
             chromeos::system::kFirmwareWriteProtectBootValueOn) {
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
          shill::kTypeBluetooth,
          em::NetworkInterface::TYPE_BLUETOOTH,
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

  // Don't write any network state if we aren't in a kiosk or public session.
  if (!GetAutoLaunchedKioskSessionInfo() &&
      !user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
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
      chromeos::ChromeUserManager::Get()->GetUsers();

  bool anything_reported = false;
  for (auto* user : users) {
    // Only users with gaia accounts (regular) are reported.
    if (!user->HasGaiaAccount())
      continue;

    em::DeviceUser* device_user = status->add_users();
    if (chromeos::ChromeUserManager::Get()->ShouldReportUser(
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

  if (report_power_status_ || report_storage_status_) {
    state->FetchEMMCLifeTime(emmc_lifetime_fetcher_);
    state->FetchCrosHealthdData(cros_healthd_data_fetcher_);
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
      chromeos::KioskAppManager::Get()
          ->GetAutoLaunchAppRequiredPlatformVersion();
  if (required_platform_version_string.empty())
    return false;

  const base::Version required_platfrom_version(
      required_platform_version_string);

  em::OsUpdateStatus* os_update_status = status->mutable_os_update_status();
  os_update_status->set_new_required_platform_version(
      required_platfrom_version.GetString());

  const update_engine::StatusResult update_engine_status =
      chromeos::DBusThreadManager::Get()
          ->GetUpdateEngineClient()
          ->GetLastStatus();

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

  if (platform_version == required_platfrom_version) {
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

    chromeos::KioskAppManager::App app_info;
    if (chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                                 &app_info)) {
      running_kiosk_app->set_required_platform_version(
          app_info.required_platform_version);
    }
  } else if (account->type == policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    // Use package name as app ID for ARC Kiosks.
    running_kiosk_app->set_app_id(account->arc_kiosk_app_info.package_name());
  } else {
    NOTREACHED();
  }
  return true;
}

void DeviceStatusCollector::GetStatusAsync(
    const StatusCollectorCallback& response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());
  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<DeviceStatusCollectorState> state(
      new DeviceStatusCollectorState(task_runner_, response));
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
  activity_storage_->TrimActivityPeriods(last_reported_day_,
                                         duration_for_last_reported_day_,
                                         std::numeric_limits<int64_t>::max());
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

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(const std::string& version) {
  firmware_version_ = version;
}

void DeviceStatusCollector::OnTpmVersion(
    const chromeos::CryptohomeClient::TpmVersionInfo& tpm_version_info) {
  tpm_version_info_ = tpm_version_info;
}

}  // namespace policy
