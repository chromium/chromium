// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

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
      public chromeos::PowerManagerClient::Observer,
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

  EventBasedStatusReportingService(const EventBasedStatusReportingService&) =
      delete;
  EventBasedStatusReportingService& operator=(
      const EventBasedStatusReportingService&) = delete;

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
  void SuspendDone(base::TimeDelta duration) override;

  // ScreenTimeController::Observer:
  void UsageTimeLimitWarning() override;

 private:
  friend class EventBasedStatusReportingServiceTest;

  void RequestStatusReport(StatusReportEvent event);

  void LogStatusReportEventUMA(StatusReportEvent event);

  // KeyedService:
  void Shutdown() override;

  const raw_ptr<content::BrowserContext, DanglingUntriaged> context_;
  bool session_just_started_ = true;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
