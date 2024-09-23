// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/event_based_status_reporting_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/network_service_instance.h"

namespace ash {

namespace {

const std::string StatusReportEventToString(
    EventBasedStatusReportingService::StatusReportEvent event) {
  switch (event) {
    case EventBasedStatusReportingService::StatusReportEvent::kAppInstalled:
      return "Request status report due to an app install.";
    case EventBasedStatusReportingService::StatusReportEvent::kAppUpdated:
      return "Request status report due to an app update.";
    case EventBasedStatusReportingService::StatusReportEvent::kSessionActive:
      return "Request status report due to a unlock screen.";
    case EventBasedStatusReportingService::StatusReportEvent::kSessionLocked:
      return "Request status report due to a lock screen.";
    case EventBasedStatusReportingService::StatusReportEvent::kDeviceOnline:
      return "Request status report due to device going online.";
    case EventBasedStatusReportingService::StatusReportEvent::kSuspendDone:
      return "Request status report after a suspend has been completed.";
    case EventBasedStatusReportingService::StatusReportEvent::
        kUsageTimeLimitWarning:
      return "Request status report before usage time limit finish.";
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

// static
constexpr char EventBasedStatusReportingService::kUMAStatusReportEvent[];

EventBasedStatusReportingService::EventBasedStatusReportingService(
    content::BrowserContext* context)
    : context_(context) {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (arc_app_prefs)
    arc_app_prefs->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  ScreenTimeControllerFactory::GetForBrowserContext(context_)->AddObserver(
      this);
}

EventBasedStatusReportingService::~EventBasedStatusReportingService() = default;

void EventBasedStatusReportingService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  RequestStatusReport(StatusReportEvent::kAppInstalled);
}

void EventBasedStatusReportingService::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  RequestStatusReport(StatusReportEvent::kAppUpdated);
}

void EventBasedStatusReportingService::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "EventBasedStatusReportingService::OnSessionStateChanged");
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();

  // Do not request status report when the device has just started the session
  // because not all components may be ready yet, which may cause an incomplete
  // status report.
  if (session_just_started_ &&
      session_state == session_manager::SessionState::ACTIVE) {
    session_just_started_ = false;
    return;
  }

  if (session_state == session_manager::SessionState::ACTIVE) {
    RequestStatusReport(StatusReportEvent::kSessionActive);
  } else if (session_state == session_manager::SessionState::LOCKED) {
    RequestStatusReport(StatusReportEvent::kSessionLocked);
  }
}

void EventBasedStatusReportingService::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type != network::mojom::ConnectionType::CONNECTION_NONE)
    RequestStatusReport(StatusReportEvent::kDeviceOnline);
}

void EventBasedStatusReportingService::SuspendDone(base::TimeDelta duration) {
  RequestStatusReport(StatusReportEvent::kSuspendDone);
}

void EventBasedStatusReportingService::UsageTimeLimitWarning() {
  RequestStatusReport(StatusReportEvent::kUsageTimeLimitWarning);
}

void EventBasedStatusReportingService::RequestStatusReport(
    StatusReportEvent event) {
  VLOG(1) << StatusReportEventToString(event);
  bool was_scheduled =
      ChildStatusReportingServiceFactory::GetForBrowserContext(context_)
          ->RequestImmediateStatusReport();
  if (was_scheduled)
    LogStatusReportEventUMA(event);
}

void EventBasedStatusReportingService::LogStatusReportEventUMA(
    StatusReportEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kUMAStatusReportEvent, event);
}

void EventBasedStatusReportingService::Shutdown() {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (arc_app_prefs)
    arc_app_prefs->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  ScreenTimeControllerFactory::GetForBrowserContext(context_)->RemoveObserver(
      this);
}

}  // namespace ash
