// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Requests status report when events relevant to supervision features happen.
// The events that are triggers to status report are:
//     * App install
//     * App update
//     * Device lock
//     * Device unlock
//     * Device connected
//     * Device returns from suspend mode
//     * Device is about to lock by usage time limit
class EventBasedStatusReportingService
    : public KeyedService,
      public ArcAppListPrefs::Observer,
      public session_manager::SessionManagerObserver,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public PowerManagerClient::Observer,
      public ScreenTimeController::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class StatusReportEvent {
    kAppInstalled = 0,
    kAppUpdated = 1,
    kSessionActive = 2,
    kSessionLocked = 3,
    kDeviceOnline = 4,
    kSuspendDone = 5,
    kUsageTimeLimitWarning = 6,
    kMaxValue = kUsageTimeLimitWarning,
  };

  // Histogram to log events that triggered status report.
  static constexpr char kUMAStatusReportEvent[] =
      "Supervision.StatusReport.Event";

  explicit EventBasedStatusReportingService(content::BrowserContext* context);
  ~EventBasedStatusReportingService() override;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& duration) override;

  // ScreenTimeController::Observer:
  void UsageTimeLimitWarning() override;

 private:
  friend class EventBasedStatusReportingServiceTest;

  void RequestStatusReport(StatusReportEvent event);

  void LogStatusReportEventUMA(StatusReportEvent event);

  // KeyedService:
  void Shutdown() override;

  content::BrowserContext* const context_;
  bool session_just_started_ = true;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
