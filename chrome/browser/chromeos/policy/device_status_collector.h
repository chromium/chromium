// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_

#include <stdint.h>

#include <memory>
#include <string>
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
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_member.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/base/idle/idle.h"

namespace chromeos {
class CrosSettings;
namespace system {
class StatisticsProvider;
}
}

namespace cryptohome {
struct TpmStatusInfo;
}

namespace user_manager {
class User;
}

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace policy {

struct DeviceLocalAccount;
class GetStatusState;

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

// Collects and summarizes the status of an enterprised-managed ChromeOS device.
class DeviceStatusCollector : public session_manager::SessionManagerObserver,
                              public chromeos::PowerManagerClient::Observer {
 public:
  using VolumeInfoFetcher = base::Callback<
    std::vector<enterprise_management::VolumeInfo>(
        const std::vector<std::string>& mount_points)>;

  // Reads the first CPU line from /proc/stat. Returns an empty string if
  // the cpu data could not be read. Broken out into a callback to enable
  // mocking for tests.
  //
  // The format of this line from /proc/stat is:
  //   cpu  user_time nice_time system_time idle_time
  using CPUStatisticsFetcher = base::Callback<std::string(void)>;

  // Reads CPU temperatures from /sys/class/hwmon/hwmon*/temp*_input and
  // appropriate labels from /sys/class/hwmon/hwmon*/temp*_label.
  using CPUTempFetcher =
      base::Callback<std::vector<enterprise_management::CPUTempInfo>()>;

  // Passed into asynchronous mojo interface for communicating with Android.
  using AndroidStatusReceiver =
      base::Callback<void(const std::string&, const std::string&)>;
  // Calls the enterprise reporting mojo interface, passing over the
  // AndroidStatusReceiver. Returns false if the mojo interface isn't available,
  // in which case no asynchronous query is emitted and the android status query
  // fails synchronously. The |AndroidStatusReceiver| is not called in this
  // case.
  using AndroidStatusFetcher =
      base::Callback<bool(const AndroidStatusReceiver&)>;

  // Format of the function that asynchronously receives TpmStatusInfo.
  using TpmStatusReceiver = base::OnceCallback<void(const TpmStatusInfo&)>;
  // Gets the TpmStatusInfo and passes it to TpmStatusReceiver.
  using TpmStatusFetcher = base::RepeatingCallback<void(TpmStatusReceiver)>;

  // Called in the UI thread after the device and session status have been
  // collected asynchronously in GetDeviceAndSessionStatusAsync. Null pointers
  // indicate errors or that device or session status reporting is disabled.
  using StatusCallback = base::Callback<void(
      std::unique_ptr<enterprise_management::DeviceStatusReportRequest>,
      std::unique_ptr<enterprise_management::SessionStatusReportRequest>)>;

  // Constructor. Callers can inject their own *Fetcher callbacks, e.g. for unit
  // testing. A null callback can be passed for any *Fetcher parameter, to use
  // the default implementation. These callbacks are always executed on Blocking
  // Pool. Caller is responsible for passing already initialized |pref_service|.
  // |activity_day_start| indicates what time does the new day start for
  // activity reporting daily data aggregation. It is represented by the
  // distance from midnight. If |is_enterprise_device| additional enterprise
  // relevant status data will be reported.
  DeviceStatusCollector(PrefService* pref_service,
                        chromeos::system::StatisticsProvider* provider,
                        const VolumeInfoFetcher& volume_info_fetcher,
                        const CPUStatisticsFetcher& cpu_statistics_fetcher,
                        const CPUTempFetcher& cpu_temp_fetcher,
                        const AndroidStatusFetcher& android_status_fetcher,
                        const TpmStatusFetcher& tpm_status_fetcher,
                        base::TimeDelta activity_day_start,
                        bool is_enterprise_reporting);
  ~DeviceStatusCollector() override;

  // Gathers device and session status information and calls the passed response
  // callback. Null pointers passed into the response indicate errors or that
  // device or session status reporting is disabled.
  virtual void GetDeviceAndSessionStatusAsync(const StatusCallback& response);

  // Called after the status information has successfully been submitted to
  // the server.
  virtual void OnSubmittedSuccessfully();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the DeviceLocalAccount associated with the currently active
  // kiosk session, if the session was auto-launched with zero delay
  // (this enables functionality such as network reporting).
  // Virtual to allow mocking.
  virtual std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo();

  bool report_activity_times() const { return report_activity_times_; }
  bool report_network_interfaces() const { return report_network_interfaces_; }
  bool report_users() const { return report_users_; }
  bool report_hardware_status() const { return report_hardware_status_; }

  // How often, in seconds, to poll to see if the user is idle.
  static const unsigned int kIdlePollIntervalSeconds = 30;

  // The total number of hardware resource usage samples cached internally.
  static const unsigned int kMaxResourceUsageSamples = 10;

  // Returns the amount of time the child has used so far today. If the user is
  // not a child or if there is no user logged in, it returns 0.
  base::TimeDelta GetActiveChildScreenTime();

 protected:
  // Check whether the user has been idle for a certain period of time.
  virtual void CheckIdleState();

  // Used instead of base::Time::Now(), to make testing possible.
  virtual base::Time GetCurrentTime();

  // Callback which receives the results of the idle state check.
  void IdleStateCallback(ui::IdleState state);

  // Gets the version of the passed app. Virtual to allow mocking.
  virtual std::string GetAppVersion(const std::string& app_id);

  // Gets the DMToken associated with a profile. Returns an empty string if no
  // DMToken could be retrieved. Virtual to allow mocking.
  virtual std::string GetDMTokenForProfile(Profile* profile);

  // Samples the current hardware resource usage to be sent up with the
  // next device status update.
  void SampleResourceUsage();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // power_manager::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;

  // power_manager::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  // power_manager::PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // The timeout in the past to store device activity.
  // This is kept in case device status uploads fail for a number of days.
  base::TimeDelta max_stored_past_activity_interval_;

  // The timeout in the future to store device activity.
  // When changing the system time and/or timezones, it's possible to record
  // activity time that is slightly in the future.
  base::TimeDelta max_stored_future_activity_interval_;

  // Updates the child's active time.
  void UpdateChildUsageTime();

 private:
  class ActivityStorage;

  // Clears the cached hardware resource usage.
  void ClearCachedResourceUsage();

  // Callbacks from chromeos::VersionLoader.
  void OnOSVersion(const std::string& version);
  void OnOSFirmware(const std::string& version);
  void OnTpmVersion(
      const chromeos::CryptohomeClient::TpmVersionInfo& tpm_version_info);

  void GetDeviceStatus(scoped_refptr<GetStatusState> state);
  void GetSessionStatus(scoped_refptr<GetStatusState> state);

  bool GetSessionStatusForUser(
      scoped_refptr<GetStatusState> state,
      enterprise_management::SessionStatusReportRequest* status,
      const user_manager::User* user);
  // Helpers for the various portions of DEVICE STATUS. Return true if they
  // actually report any status. Functions that queue async queries take
  // a |GetStatusState| instance.
  bool GetActivityTimes(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetVersionInfo(enterprise_management::DeviceStatusReportRequest* status);
  bool GetBootMode(enterprise_management::DeviceStatusReportRequest* status);
  bool GetWriteProtectSwitch(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetNetworkInterfaces(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetUsers(enterprise_management::DeviceStatusReportRequest* status);
  bool GetHardwareStatus(
      enterprise_management::DeviceStatusReportRequest* status,
      scoped_refptr<GetStatusState> state);  // Queues async queries!
  bool GetOsUpdateStatus(
      enterprise_management::DeviceStatusReportRequest* status);
  bool GetRunningKioskApp(
      enterprise_management::DeviceStatusReportRequest* status);

  // Helpers for the various portions of SESSION STATUS. Return true if they
  // actually report any status. Functions that queue async queries take
  // a |GetStatusState| instance.
  bool GetKioskSessionStatus(
      enterprise_management::SessionStatusReportRequest* status);
  bool GetAndroidStatus(
      enterprise_management::SessionStatusReportRequest* status,
      const scoped_refptr<GetStatusState>& state);  // Queues async queries!
  bool GetCrostiniUsage(
      enterprise_management::SessionStatusReportRequest* status,
      Profile* profile);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  // Callback invoked to update our cpu usage information.
  void ReceiveCPUStatistics(const std::string& statistics);

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

  // The last time an active state check was performed.
  base::Time last_active_check_;

  // Whether the last state of the device was active. This is used for child
  // accounts only. Active is defined as having the screen turned on.
  bool last_state_active_;

  // The maximum key that went into the last report generated by
  // GetDeviceStatusAsync(), and the duration for it. This is used to trim
  // the stored data in OnSubmittedSuccessfully(). Trimming is delayed so
  // unsuccessful uploads don't result in dropped data.
  int64_t last_reported_day_ = 0;
  int duration_for_last_reported_day_ = 0;

  base::RepeatingTimer idle_poll_timer_;
  base::RepeatingTimer update_child_usage_timer_;
  base::RepeatingTimer resource_usage_sampling_timer_;

  std::string os_version_;
  std::string firmware_version_;
  chromeos::CryptohomeClient::TpmVersionInfo tpm_version_info_;

  struct ResourceUsage {
    // Sample of percentage-of-CPU-used.
    int cpu_usage_percent;

    // Amount of free RAM (measures raw memory used by processes, not internal
    // memory waiting to be reclaimed by GC).
    int64_t bytes_of_ram_free;
  };

  // Samples of resource usage (contains multiple samples taken
  // periodically every kHardwareStatusSampleIntervalSeconds).
  base::circular_deque<ResourceUsage> resource_usage_;

  // Callback invoked to fetch information about the mounted disk volumes.
  VolumeInfoFetcher volume_info_fetcher_;

  // Callback invoked to fetch information about cpu usage.
  CPUStatisticsFetcher cpu_statistics_fetcher_;

  // Callback invoked to fetch information about cpu temperature.
  CPUTempFetcher cpu_temp_fetcher_;

  AndroidStatusFetcher android_status_fetcher_;

  TpmStatusFetcher tpm_status_fetcher_;

  chromeos::system::StatisticsProvider* const statistics_provider_;

  chromeos::CrosSettings* const cros_settings_;

  // Power manager client. Used to listen to suspend and idle events.
  chromeos::PowerManagerClient* const power_manager_;

  // Session manager. Used to listen to session state changes.
  session_manager::SessionManager* const session_manager_;

  // Stores and filters activity periods used for reporting.
  std::unique_ptr<ActivityStorage> activity_storage_;

  // The most recent CPU readings.
  uint64_t last_cpu_active_ = 0;
  uint64_t last_cpu_idle_ = 0;

  // Cached values of the reporting settings from the device policy.
  bool report_version_info_ = false;
  bool report_activity_times_ = false;
  bool report_boot_mode_ = false;
  bool report_network_interfaces_ = false;
  bool report_users_ = false;
  bool report_hardware_status_ = false;
  bool report_kiosk_session_status_ = false;
  bool report_os_update_status_ = false;
  bool report_running_kiosk_app_ = false;

  // Whether reporting is for enterprise or consumer.
  bool is_enterprise_reporting_ = false;

  // New day start time used to separate the children usage time into different
  // days.
  const base::TimeDelta activity_day_start_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      version_info_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      activity_times_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      boot_mode_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      network_interfaces_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      users_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      hardware_status_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      session_status_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      os_update_status_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      running_kiosk_app_subscription_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Task runner in the creation thread where responses are sent to.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DeviceStatusCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_STATUS_COLLECTOR_H_
