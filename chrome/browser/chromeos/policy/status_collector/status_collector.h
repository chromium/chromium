// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/proto/device_management_backend.pb.h"

class PrefRegistrySimple;
class Profile;

namespace chromeos {
class CrosSettings;
namespace system {
class StatisticsProvider;
}
}  // namespace chromeos

namespace policy {

class ActivityStorage;
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
using StatusCollectorCallback =
    base::RepeatingCallback<void(StatusCollectorParams)>;

// Defines the API for a status collector.
class StatusCollector {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Simplifies filling the boot mode for any of the relevant status report
  // requests.
  static base::Optional<std::string> GetBootMode(
      chromeos::system::StatisticsProvider* statistics_provider);

  StatusCollector(chromeos::system::StatisticsProvider* provider,
                  chromeos::CrosSettings* cros_settings);
  virtual ~StatusCollector();

  // Gathers status information and calls the passed response callback.
  virtual void GetStatusAsync(const StatusCollectorCallback& callback) = 0;

  // Called after the status information has successfully been submitted to
  // the server.
  virtual void OnSubmittedSuccessfully() = 0;

  // Methods used to decide whether a specific categories of data should be
  // included in the reports or not. See:
  // https://cs.chromium.org/search/?q=AddDeviceReportingInfo
  virtual bool ShouldReportActivityTimes() const = 0;
  virtual bool ShouldReportNetworkInterfaces() const = 0;
  virtual bool ShouldReportUsers() const = 0;
  virtual bool ShouldReportHardwareStatus() const = 0;

  // Returns the DeviceLocalAccount associated with the currently active kiosk
  // session, if the session was auto-launched with zero delay (this enables
  // functionality such as network reporting). If it isn't a kiosk session,
  // nullptr is returned.
  virtual std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo();

 protected:
  // Gets the DMToken associated with a profile. Returns an empty string if no
  // DMToken could be retrieved. Virtual to allow mocking.
  virtual std::string GetDMTokenForProfile(Profile* profile) const;

  // Used instead of base::Time::Now(), to make testing possible.
  // TODO(crbug.com/827386): pass a Clock object and use SimpleTestClock to test
  // it.
  virtual base::Time GetCurrentTime();

  // The timeout in the past to store activity.
  // This is kept in case status uploads fail for a number of days.
  base::TimeDelta max_stored_past_activity_interval_;

  // The timeout in the future to store activity.
  // When changing the system time and/or timezones, it's possible to record
  // activity time that is slightly in the future.
  base::TimeDelta max_stored_future_activity_interval_;

  chromeos::system::StatisticsProvider* const statistics_provider_;

  chromeos::CrosSettings* const cros_settings_;

  // Cached values of the reporting settings.
  bool report_version_info_ = false;
  bool report_activity_times_ = false;
  bool report_boot_mode_ = false;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      version_info_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      boot_mode_subscription_;

  // Task runner in the creation thread where responses are sent to.
  scoped_refptr<base::SequencedTaskRunner> task_runner_ = nullptr;
  // TODO(crbug.com/827386): check if it is possible to use the SequenceChecker
  // instead.
  base::ThreadChecker thread_checker_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_H_
