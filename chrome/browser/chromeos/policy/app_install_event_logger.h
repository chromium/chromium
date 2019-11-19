// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_collector.h"
#include "components/policy/core/common/policy_service.h"

class Profile;

namespace base {
class Value;
}

namespace enterprise_management {
class AppInstallReportLogEvent;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

class PolicyMap;
struct PolicyNamespace;

// Ensures that events relevant to app push-installs are logged. Three types of
// events are logged directly by this class:
// * When an app is added to the push-install list in policy, the start of its
//   push-install process is logged.
// * When an app disappears from the non-compliance list returned by CloudDPC,
//   the successful end of its push-install process is logged.
// * When an app is removed from the push-install list in policy, the end of its
//   push-install process is logged.
//
// Additionally, an |AppInstallEventLogCollector| is instantiated to collect
// detailed logs of the push-install process whenever there is at least one
// pending push-install request.
class AppInstallEventLogger : public AppInstallEventLogCollector::Delegate,
                              public policy::PolicyService::Observer,
                              public arc::ArcPolicyBridge::Observer {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every app in |packages|.
    virtual void Add(
        const std::set<std::string>& packages,
        const enterprise_management::AppInstallReportLogEvent& event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  AppInstallEventLogger(Delegate* delegate, Profile* profile);
  ~AppInstallEventLogger() override;

  // Registers the prefs used to keep track of push-installs that have been
  // requested and not yet completed.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Clears all data related to app-install event log collection for |profile|.
  // Must not be called while an |AppInstallEventLogger| exists for |profile|.
  static void Clear(Profile* profile);

  // AppInstallEventLogCollector::Delegate:
  void AddForAllPackages(
      std::unique_ptr<enterprise_management::AppInstallReportLogEvent> event)
      override;
  void Add(const std::string& package,
           bool gather_disk_space_info,
           std::unique_ptr<enterprise_management::AppInstallReportLogEvent>
               event) override;

  // policy::PolicyService::Observer:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // arc::ArcPolicyBridge::Observer:
  void OnPolicySent(const std::string& policy) override;
  void OnComplianceReportReceived(
      const base::Value* compliance_report) override;

 private:
  // Loads a list of packages from a pref.
  std::set<std::string> GetPackagesFromPref(const std::string& pref_name) const;

  // Stores a list of packages into a pref.
  void SetPref(const std::string& pref_name,
               const std::set<std::string>& packages);

  // Informs the existing |log_collector_| that the list of pending app
  // push-install requests has changed or instantiates a new |log_collector_| if
  // none exists yet.
  void UpdateCollector(const std::set<std::string>& pending);

  // Destroys the |log_collector_|, if it exists.
  void StopCollector();

  // Extracts the list of app push-install requests from |policy|, logs the
  // cancellation of any pending push-installs that are no longer in |policy|
  // and updates the |log_collector_|.
  void EvaluatePolicy(const policy::PolicyMap& policy, bool initial);

  // Adds information about total and free disk space to |event|, then adds
  // |event| to the log for every app in |packages|.
  void AddForSetOfPackagesWithDiskSpaceInfo(
      const std::set<std::string>& packages,
      std::unique_ptr<enterprise_management::AppInstallReportLogEvent> event);

  // Adds |event| to the log for every app in |packages|.
  void AddForSetOfPackages(
      const std::set<std::string>& packages,
      std::unique_ptr<enterprise_management::AppInstallReportLogEvent> event);

  // The delegate that events are forwarded to for inclusion in the log.
  Delegate* const delegate_;

  // The profile whose app push-install requests to log.
  Profile* const profile_;

  // Whether |this| has set itself up as observer of other classes and needs to
  // remove itself as observer in the destructor.
  bool observing_ = false;

  // The app push-install requests that were most recently sent to CloudDPC.
  std::set<std::string> requested_in_arc_;

  // The |AppInstallEventLogCollector| that collects detailed logs of the
  // push-install process. Non-|nullptr| whenever there are one or more pending
  // app push-install requests.
  std::unique_ptr<AppInstallEventLogCollector> log_collector_;

  // Weak factory used to reference |this| from background tasks.
  base::WeakPtrFactory<AppInstallEventLogger> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogger);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_
