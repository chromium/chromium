// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_DEVICE_STATUS_COLLECTOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/status_collector/app_info_generator.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_member.h"
#include "ui/base/idle/idle.h"

namespace chromeos {
namespace system {
class StatisticsProvider;
}
}  // namespace chromeos

namespace cryptohome {
struct TpmStatusInfo;
}

namespace power_manager {
class PowerSupplyProperties;
}

namespace user_manager {
class User;
}

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace policy {

class EnterpriseActivityStorage;
struct DeviceLocalAccount;
class DeviceStatusCollectorState;

// Enum used to define which data the CrosHealthdDataFetcher should collect.
enum class CrosHealthdCollectionMode { kFull, kBattery };

// Holds TPM status info.  Cf. TpmStatusInfo in device_management_backend.proto.
struct TpmStatusInfo {
  TpmStatusInfo();
  TpmStatusInfo(const TpmStatusInfo&);
  TpmStatusInfo(bool enabled,
                bool owned,
                bool initialized,
                bool attestation_prepared,
                bool attestation_enrolled,
                int32_t dictionary_attack_counter,
                int32_t dictionary_attack_threshold,
                bool dictionary_attack_lockout_in_effect,
                int32_t dictionary_attack_lockout_seconds_remaining,
                bool boot_lockbox_finalized);
  ~TpmStatusInfo();

  bool enabled = false;
  bool owned = false;
  bool initialized = false;
  bool attestation_prepared = false;
  bool attestation_enrolled = false;
  int32_t dictionary_attack_counter = 0;
  int32_t dictionary_attack_threshold = 0;
  bool dictionary_attack_lockout_in_effect = false;
  int32_t dictionary_attack_lockout_seconds_remaining = 0;
  bool boot_lockbox_finalized = false;
};

// Sampled hardware measurement data for single time point.
class SampledData {
 public:
  SampledData();
  ~SampledData();

  // Sampling timestamp.
  base::Time timestamp;
  // Battery samples for each battery.
  std::map<std::string, enterprise_management::BatterySample> battery_samples;
  // Thermal samples for each thermal point.
  std::map<std::string, enterprise_management::ThermalSample> thermal_samples;
  // CPU thermal samples.
  std::map<std::string, enterprise_management::CPUTempInfo> cpu_samples;

  DISALLOW_COPY_AND_ASSIGN(SampledData);
};

// Collects and summarizes the status of an enterprise-managed ChromeOS device.
class DeviceStatusCollector : public StatusCollector,
                              public chromeos::PowerManagerClient::Observer {
 public:
  using VolumeInfoFetcher =
      base::RepeatingCallback<std::vector<enterprise_management::VolumeInfo>(
          const std::vector<std::string>& mount_points)>;

  // Reads the first CPU line from /proc/stat. Returns an empty string if
  // the cpu data could not be read. Broken out into a callback to enable
  // mocking for tests.
  //
  // The format of this line from /proc/stat is:
  //   cpu  user_time nice_time system_time idle_time
  using CPUStatisticsFetcher = base::RepeatingCallback<std::string(void)>;

  // Reads CPU temperatures from /sys/class/hwmon/hwmon*/temp*_input and
  // appropriate labels from /sys/class/hwmon/hwmon*/temp*_label.
  using CPUTempFetcher = base::RepeatingCallback<
      std::vector<enterprise_management::CPUTempInfo>()>;

  // Format of the function that asynchronously receives TpmStatusInfo.
  using TpmStatusReceiver = base::OnceCallback<void(const TpmStatusInfo&)>;
  // Gets the TpmStatusInfo and passes it to TpmStatusReceiver.
  using TpmStatusFetcher = base::RepeatingCallback<void(TpmStatusReceiver)>;

  // Format of the function that asynchronously receives data from cros_healthd.
  using CrosHealthdDataReceiver = base::OnceCallback<void(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr,
      const base::circular_deque<std::unique_ptr<SampledData>>&)>;
  // Gets the data from cros_healthd and passes it to CrosHealthdDataReceiver.
  using CrosHealthdDataFetcher =
      base::RepeatingCallback<void(CrosHealthdCollectionMode,
                                   CrosHealthdDataReceiver)>;

  // Asynchronously receives the graphics status.
  using GraphicsStatusReceiver =
      base::OnceCallback<void(const enterprise_management::GraphicsStatus&)>;

  // Gets the display and graphics adapter information reported to the browser
  // by the GPU process.
  using GraphicsStatusFetcher =
      base::RepeatingCallback<void(GraphicsStatusReceiver)>;

  // Format of the function that asynchronously receives CrashReportInfo.
  using CrashReportInfoReceiver = base::OnceCallback<void(
      const std::vector<enterprise_management::CrashReportInfo>&)>;

  // Gets the crash report information stored on the local device.
  using CrashReportInfoFetcher =
      base::RepeatingCallback<void(CrashReportInfoReceiver)>;

  // Reads EMMC usage lifetime from /var/log/storage_info.txt
  using EMMCLifetimeFetcher =
      base::RepeatingCallback<enterprise_management::DiskLifetimeEstimation(
          void)>;
  // Reads the stateful partition info from /home/.shadow
  using StatefulPartitionInfoFetcher =
      base::RepeatingCallback<enterprise_management::StatefulPartitionInfo()>;

  // Constructor. Callers can inject their own *Fetcher callbacks, e.g. for unit
  // testing. A null callback can be passed for any *Fetcher parameter, to use
  // the default implementation. These callbacks are always executed on Blocking
  // Pool. Caller is responsible for passing already initialized |pref_service|.
  DeviceStatusCollector(
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
      base::Clock* clock = base::DefaultClock::GetInstance());

  // Constructor with default callbacks. These callbacks are always executed on
  // Blocking Pool. Caller is responsible for passing already initialized
  // |pref_service|.
  DeviceStatusCollector(PrefService* pref_service,
                        chromeos::system::StatisticsProvider* provider);

  ~DeviceStatusCollector() override;

  // StatusCollector:
  void GetStatusAsync(StatusCollectorCallback response) override;
  void OnSubmittedSuccessfully() override;
  bool ShouldReportActivityTimes() const override;
  bool ShouldReportNetworkInterfaces() const override;
  bool ShouldReportUsers() const override;
  bool ShouldReportHardwareStatus() const override;
  bool ShouldReportCrashReportInfo() const override;
  bool ShouldReportAppInfoAndActivity() const override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  AffiliatedSessionService* GetAffiliatedSessionServiceForTesting() {
    return &affiliated_session_service_;
  }

  // How often to poll to see if the user is idle.
  static constexpr base::TimeDelta kIdlePollInterval =
      base::TimeDelta::FromSeconds(30);

  // The total number of hardware resource usage samples cached internally.
  static const unsigned int kMaxResourceUsageSamples = 10;

 protected:
  using PowerStatusCallback = base::OnceCallback<void(
      const power_manager::PowerSupplyProperties& prop)>;

  // Check whether the user has been idle for a certain period of time.
  virtual void CheckIdleState();

  // Handles the results of the idle state check.
  void ProcessIdleState(ui::IdleState state);

  // Gets the version of the passed app. Virtual to allow mocking.
  virtual std::string GetAppVersion(const std::string& app_id);

  // Samples the current hardware resource usage to be sent up with the
  // next device status update.
  void SampleResourceUsage();

  // power_manager::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& prop) override;

 private:
  // Callbacks used during sampling data collection, that allows to pass
  // additional data using partial function application.
  using SamplingProbeResultCallback =
      base::OnceCallback<void(chromeos::cros_healthd::mojom::TelemetryInfoPtr)>;
  using SamplingCallback = base::OnceCallback<void()>;

  // Clears the cached hardware resource usage.
  void ClearCachedResourceUsage();

  // Callbacks from chromeos::VersionLoader.
  void OnOSVersion(const std::string& version);
  void OnOSFirmware(std::pair<const std::string&, const std::string&> version);

  // Callbacks from `chromeos::TpmManagerClient`.
  void OnGetTpmVersion(const ::tpm_manager::GetVersionInfoReply& reply);

  void GetDeviceStatus(scoped_refptr<DeviceStatusCollectorState> state);
  void GetSessionStatus(scoped_refptr<DeviceStatusCollectorState> state);

  bool GetSessionStatusForUser(
      scoped_refptr<DeviceStatusCollectorState> state,
      enterprise_management::SessionStatusReportRequest* status,
      const user_manager::User* user);
  // Helpers for the various portions of DEVICE STATUS. Return true if they
  // actually report any status. Functions that queue async queries take
  // a |DeviceStatusCollectorState| instance.
  bool GetActivityTimes(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetVersionInfo(enterprise_management::DeviceStatusReportRequest* status);
  bool GetWriteProtectSwitch(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetNetworkInterfaces(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetUsers(enterprise_management::DeviceStatusReportRequest* status);
  bool GetHardwareStatus(scoped_refptr<DeviceStatusCollectorState>
                             state);  // Queues async queries!
  bool GetOsUpdateStatus(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetRunningKioskApp(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetGraphicsStatus(scoped_refptr<DeviceStatusCollectorState>
                             state);  // Queues async queries!
  bool GetCrashReportInfo(scoped_refptr<DeviceStatusCollectorState>
                              state);  // Queues async queries!

  // Helpers for the various portions of SESSION STATUS. Return true if they
  // actually report any status. Functions that queue async queries take
  // a |DeviceStatusCollectorState| instance.
  bool GetKioskSessionStatus(
      enterprise_management::SessionStatusReportRequest* status);
  bool GetAndroidStatus(
      enterprise_management::SessionStatusReportRequest* status,
      const scoped_refptr<DeviceStatusCollectorState>&
          state);  // Queues async queries!
  bool GetCrostiniUsage(
      enterprise_management::SessionStatusReportRequest* status,
      Profile* profile);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  // Callback invoked to update our cpu usage information.
  void ReceiveCPUStatistics(const std::string& statistics);

  // Callback for CrosHealthd that samples probe live data. |callback| will
  // be called once all sampling is finished.
  void SampleProbeData(std::unique_ptr<SampledData> sample,
                       SamplingProbeResultCallback callback,
                       chromeos::cros_healthd::mojom::TelemetryInfoPtr result);

  // Callback triggered from PowerManagedClient that samples battery discharge
  // rate. |callback| will be called once all sampling is finished.
  void SampleDischargeRate(std::unique_ptr<SampledData> sample,
                           SamplingCallback callback,
                           const power_manager::PowerSupplyProperties& prop);

  // Callback invoked to update our cpu temperature information.
  void ReceiveCPUTemperature(std::unique_ptr<SampledData> sample,
                             SamplingCallback callback,
                             std::vector<enterprise_management::CPUTempInfo>);

  // Final sampling step that records data sample, invokes |callback|.
  void AddDataSample(std::unique_ptr<SampledData> sample,
                     SamplingCallback callback);

  // CrosHealthdDataReceiver interface implementation, fetches data from
  // cros_healthd and passes it to |callback|. The data collected depends on the
  // collection |mode|.
  void FetchCrosHealthdData(CrosHealthdCollectionMode mode,
                            CrosHealthdDataReceiver callback);

  // Callback for CrosHealthd that performs final sampling and
  // actually invokes |callback|.
  void OnProbeDataFetched(
      CrosHealthdDataReceiver callback,
      chromeos::cros_healthd::mojom::TelemetryInfoPtr reply);

  // Returns true if data (e.g. CPU info, power status, etc.) should be fetched
  // from cros_healthd.
  bool ShouldFetchCrosHealthdData() const;

  // Callback invoked when reporting users pref is changed.
  void ReportingUsersChanged();

  // Returns user's email if it should be included in the activity reports or
  // empty string otherwise. Primary user is used as unique identifier of a
  // single session, even for multi-user sessions.
  std::string GetUserForActivityReporting() const;

  // Returns whether users' email addresses should be included in activity
  // reports.
  bool IncludeEmailsInActivityReports() const;

  // Pref service that is mainly used to store activity periods for reporting.
  PrefService* const pref_service_;

  // The last time an idle state check was performed.
  base::Time last_idle_check_;

  // End timestamp of the latest activity that went into the last report
  // generated by GetStatusAsync(). Used to trim the stored data in
  // OnSubmittedSuccessfully(). Trimming is delayed so unsuccessful uploads
  // don't result in dropped data.
  int64_t last_reported_end_timestamp_ = 0;

  // Time when GetStatusAsync() is called. Used to close open app
  // activity just prior to reporting so the report can include the most
  // up-to-date activity.
  base::Time last_requested_;

  base::RepeatingTimer idle_poll_timer_;
  base::RepeatingTimer resource_usage_sampling_timer_;

  std::string os_version_;
  std::string firmware_version_;
  std::string firmware_fetch_error_;
  ::tpm_manager::GetVersionInfoReply tpm_version_reply_;

  struct ResourceUsage {
    // Sample of percentage-of-CPU-used.
    int cpu_usage_percent;

    // Amount of free RAM (measures raw memory used by processes, not internal
    // memory waiting to be reclaimed by GC).
    int64_t bytes_of_ram_free;

    // Sampling timestamp.
    base::Time timestamp;
  };

  // Samples of resource usage (contains multiple samples taken
  // periodically every kHardwareStatusSampleIntervalSeconds).
  base::circular_deque<ResourceUsage> resource_usage_;

  // Samples of probe data (contains multiple samples taken
  // periodically every kHardwareStatusSampleIntervalSeconds)
  base::circular_deque<std::unique_ptr<SampledData>> sampled_data_;

  // Callback invoked to fetch information about the mounted disk volumes.
  VolumeInfoFetcher volume_info_fetcher_;

  // Callback invoked to fetch information about cpu usage.
  CPUStatisticsFetcher cpu_statistics_fetcher_;

  // Callback invoked to fetch information about cpu temperature.
  CPUTempFetcher cpu_temp_fetcher_;

  AndroidStatusFetcher android_status_fetcher_;

  TpmStatusFetcher tpm_status_fetcher_;

  EMMCLifetimeFetcher emmc_lifetime_fetcher_;

  StatefulPartitionInfoFetcher stateful_partition_info_fetcher_;

  CrosHealthdDataFetcher cros_healthd_data_fetcher_;

  GraphicsStatusFetcher graphics_status_fetcher_;

  CrashReportInfoFetcher crash_report_info_fetcher_;

  PowerStatusCallback power_status_callback_;

  // Power manager client. Used to listen to power changed events.
  chromeos::PowerManagerClient* const power_manager_;

  // The most recent CPU readings.
  uint64_t last_cpu_active_ = 0;
  uint64_t last_cpu_idle_ = 0;

  // Cached values of the reporting settings. These are enterprise only. There
  // are common ones in StatusCollector interface.
  bool report_network_interfaces_ = false;
  bool report_users_ = false;
  bool report_hardware_status_ = false;
  bool report_kiosk_session_status_ = false;
  bool report_os_update_status_ = false;
  bool report_running_kiosk_app_ = false;
  bool report_power_status_ = false;
  bool report_storage_status_ = false;
  bool report_board_status_ = false;
  bool report_cpu_info_ = false;
  bool report_graphics_status_ = false;
  bool report_timezone_info_ = false;
  bool report_memory_info_ = false;
  bool report_backlight_info_ = false;
  bool report_crash_report_info_ = false;
  bool report_bluetooth_info_ = false;
  bool report_fan_info_ = false;
  bool report_vpd_info_ = false;
  bool report_app_info_ = false;
  bool report_system_info_ = false;
  bool stat_reporting_pref_ = false;

  base::CallbackListSubscription activity_times_subscription_;
  base::CallbackListSubscription network_interfaces_subscription_;
  base::CallbackListSubscription users_subscription_;
  base::CallbackListSubscription hardware_status_subscription_;
  base::CallbackListSubscription session_status_subscription_;
  base::CallbackListSubscription os_update_status_subscription_;
  base::CallbackListSubscription running_kiosk_app_subscription_;
  base::CallbackListSubscription power_status_subscription_;
  base::CallbackListSubscription storage_status_subscription_;
  base::CallbackListSubscription board_status_subscription_;
  base::CallbackListSubscription cpu_info_subscription_;
  base::CallbackListSubscription graphics_status_subscription_;
  base::CallbackListSubscription timezone_info_subscription_;
  base::CallbackListSubscription memory_info_subscription_;
  base::CallbackListSubscription backlight_info_subscription_;
  base::CallbackListSubscription crash_report_info_subscription_;
  base::CallbackListSubscription bluetooth_info_subscription_;
  base::CallbackListSubscription fan_info_subscription_;
  base::CallbackListSubscription vpd_info_subscription_;
  base::CallbackListSubscription system_info_subscription_;
  base::CallbackListSubscription app_info_subscription_;
  base::CallbackListSubscription stats_reporting_pref_subscription_;

  AffiliatedSessionService affiliated_session_service_;

  AppInfoGenerator app_info_generator_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Stores and filters activity periods used for reporting.
  std::unique_ptr<EnterpriseActivityStorage> activity_storage_;

  base::WeakPtrFactory<DeviceStatusCollector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_DEVICE_STATUS_COLLECTOR_H_
