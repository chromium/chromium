// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_COLLECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_COLLECTOR_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "ash/components/arc/mojom/policy.mojom-forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_collector_base.h"
#include "chrome/browser/profiles/profile.h"

class Profile;

namespace enterprise_management {
class AppInstallReportLogEvent;
}
namespace policy {

// Listens for and logs events related to app push-installs.
class ArcAppInstallEventLogCollector : public InstallEventLogCollectorBase,
                                       public arc::ArcPolicyBridge::Observer,
                                       public ArcAppListPrefs::Observer {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every app whose push-install is pending.
    // The |event|'s timestamp is set to the current time if not set yet.
    virtual void AddForAllPackages(
        std::unique_ptr<enterprise_management::AppInstallReportLogEvent>
            event) = 0;

    // Adds a log entry for |package|. The |event|'s timestamp is set to the
    // current time if not set yet. If |gather_disk_space_info| is |true|,
    // information about total and free disk space is gathered in the background
    // and added to |event| before adding it to the log.
    virtual void Add(
        const std::string& package,
        bool gather_disk_space_info,
        std::unique_ptr<enterprise_management::AppInstallReportLogEvent>
            event) = 0;

    // Uses a package's installation status to update policy success rate data.
    virtual void UpdatePolicySuccessRate(const std::string& package,
                                         bool success) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  ArcAppInstallEventLogCollector(Delegate* delegate,
                                 Profile* profile,
                                 const std::set<std::string>& pending_packages);
  ~ArcAppInstallEventLogCollector() override;
  ArcAppInstallEventLogCollector(const ArcAppInstallEventLogCollector&) =
      delete;
  ArcAppInstallEventLogCollector& operator=(
      const ArcAppInstallEventLogCollector&) = delete;

  // Called whenever the list of pending app-install requests changes.
  void OnPendingPackagesChanged(const std::set<std::string>& pending_packages);

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // arc::ArcPolicyBridge::Observer:
  void OnPlayStoreLocalPolicySet(
      base::Time time,
      const std::set<std::string>& package_names) override;

  // ArcAppListPrefs::Observer:
  void OnInstallationStarted(const std::string& package_name) override;
  void OnInstallationFinished(const std::string& package_name,
                              bool success,
                              bool is_launchable_app) override;

 protected:
  // Overrides to handle events from InstallEventLogCollectorBase.
  void OnLoginInternal() override;
  void OnLogoutInternal() override;
  void OnConnectionStateChanged(network::mojom::ConnectionType type) override;

 private:
  const raw_ptr<Delegate> delegate_;

  // Set of apps whose push-install is currently pending.
  std::set<std::string> pending_packages_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_COLLECTOR_H_
