// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/device_management_backend.pb.h"

class PrefRegistrySimple;
class Profile;

namespace ash {
class CrosSettings;
namespace system {
class StatisticsProvider;
}  // namespace system
}  // namespace ash

namespace policy {

struct DeviceLocalAccount;

// Groups parameters that are necessary either directly in the
// |StatusCollectorCallback| or in async methods that work as callbacks and
// expect these exact same parameters. Absence of values indicates errors or
// that status reporting is disabled.
//
// Notice that the fields are used in different contexts, depending on the type
// of user:
// - Enterprise users: |device_status| and |session_status|.
// - Child users:
//    - During the migration away from DeviceStatusCollector:
//      |device_status|, |session_status|, |child_status|.
//    - After migration: only |child_status|.
struct StatusCollectorParams {
  StatusCollectorParams();
  ~StatusCollectorParams();

  // Move only.
  StatusCollectorParams(StatusCollectorParams&&);
  StatusCollectorParams& operator=(StatusCollectorParams&&);

  std::unique_ptr<enterprise_management::DeviceStatusReportRequest>
      device_status;
  std::unique_ptr<enterprise_management::SessionStatusReportRequest>
      session_status;
  std::unique_ptr<enterprise_management::ChildStatusReportRequest> child_status;
};

// Called in the UI thread after the statuses have been collected
// asynchronously.
using StatusCollectorCallback = base::OnceCallback<void(StatusCollectorParams)>;

// Defines the API for a status collector.
class StatusCollector {
 public:
  // Passed into asynchronous mojo interface for communicating with Android.
  using AndroidStatusReceiver =
      base::OnceCallback<void(const std::string&, const std::string&)>;
  // Calls the enterprise reporting mojo interface, passing over the
  // AndroidStatusReceiver. Returns false if the mojo interface isn't available,
  // in which case no asynchronous query is emitted and the android status query
  // fails synchronously. The |AndroidStatusReceiver| is not called in this
  // case.
  using AndroidStatusFetcher =
      base::RepeatingCallback<bool(AndroidStatusReceiver)>;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Simplifies filling the boot mode for any of the relevant status report
  // requests.
  static std::optional<std::string> GetBootMode(
      ash::system::StatisticsProvider* statistics_provider);

  StatusCollector(ash::system::StatisticsProvider* provider,
                  ash::CrosSettings* cros_settings,
                  base::Clock* clock = base::DefaultClock::GetInstance());
  virtual ~StatusCollector();

  // Gathers status information and calls the passed response callback.
  virtual void GetStatusAsync(StatusCollectorCallback callback) = 0;

  // Called after the status information has successfully been submitted to
  // the server.
  virtual void OnSubmittedSuccessfully() = 0;

  // Methods used to determine if privacy notes should be displayed in
  // management UI.
  // https://cs.chromium.org/search/?q=AddDeviceReportingInfo
  virtual bool IsReportingActivityTimes() const = 0;
  virtual bool IsReportingNetworkData() const = 0;
  virtual bool IsReportingHardwareData() const = 0;
  virtual bool IsReportingUsers() const = 0;
  virtual bool IsReportingCrashReportInfo() const = 0;
  virtual bool IsReportingAppInfoAndActivity() const = 0;

  // Returns the DeviceLocalAccount associated with the currently active kiosk
  // session, if the session was auto-launched with zero delay (this enables
  // functionality such as network reporting). If it isn't a kiosk session,
  // nullptr is returned.
  virtual std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo();

 protected:
  // Gets the DMToken associated with a profile. Returns an empty string if no
  // DMToken could be retrieved. Virtual to allow mocking.
  virtual std::string GetDMTokenForProfile(Profile* profile) const;

  // The timeout in the past to store activity.
  // This is kept in case status uploads fail for a number of days.
  base::TimeDelta max_stored_past_activity_interval_;

  // The timeout in the future to store activity.
  // When changing the system time and/or timezones, it's possible to record
  // activity time that is slightly in the future.
  base::TimeDelta max_stored_future_activity_interval_;

  const raw_ptr<ash::system::StatisticsProvider> statistics_provider_;

  const raw_ptr<ash::CrosSettings> cros_settings_;

  // Cached values of the reporting settings.
  bool report_version_info_ = false;
  bool report_activity_times_ = false;
  bool report_boot_mode_ = false;

  base::CallbackListSubscription version_info_subscription_;
  base::CallbackListSubscription boot_mode_subscription_;

  raw_ptr<base::Clock> clock_;

  // Task runner in the creation thread where responses are sent to.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // TODO(crbug.com/40569404): check if it is possible to use the
  // SequenceChecker instead.
  base::ThreadChecker thread_checker_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
