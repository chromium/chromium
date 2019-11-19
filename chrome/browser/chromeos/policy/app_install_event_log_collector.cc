// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<em::AppInstallReportLogEvent> CreateSessionChangeEvent(
    em::AppInstallReportLogEvent::SessionStateChangeType type) {
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE);
  event->set_session_state_change_type(type);
  return event;
}

bool GetOnlineState() {
  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(
          chromeos::NetworkTypePattern::Default(), true /* configured_only */,
          false /* visible_only */, 0 /* limit */, &network_state_list);
  for (const chromeos::NetworkState* network_state : network_state_list) {
    if (network_state->connection_state() == shill::kStateOnline) {
      return true;
    }
  }
  return false;
}

void SetTimestampFromTime(em::AppInstallReportLogEvent* event,
                          base::Time time) {
  event->set_timestamp((time - base::Time::UnixEpoch()).InMicroseconds());
}

}  // namespace

AppInstallEventLogCollector::AppInstallEventLogCollector(
    Delegate* delegate,
    Profile* profile,
    const std::set<std::string>& pending_packages)
    : delegate_(delegate),
      profile_(profile),
      online_(GetOnlineState()),
      pending_packages_(pending_packages) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  // Might not be available in unit test.
  arc::ArcPolicyBridge* const policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge) {
    policy_bridge->AddObserver(this);
  }
  ArcAppListPrefs* const app_prefs = ArcAppListPrefs::Get(profile_);
  if (app_prefs) {
    app_prefs->AddObserver(this);
  }
}

AppInstallEventLogCollector::~AppInstallEventLogCollector() {
  ArcAppListPrefs* const app_prefs = ArcAppListPrefs::Get(profile_);
  if (app_prefs) {
    app_prefs->RemoveObserver(this);
  }
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  arc::ArcPolicyBridge* const policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge) {
    policy_bridge->RemoveObserver(this);
  }
}

void AppInstallEventLogCollector::OnPendingPackagesChanged(
    const std::set<std::string>& pending_packages) {
  pending_packages_ = pending_packages;
}

void AppInstallEventLogCollector::AddLoginEvent() {
  // Don't log in case session is restared or recovered from crash.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser) ||
      profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    return;
  }

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGIN);
  event->set_online(online_);
  delegate_->AddForAllPackages(std::move(event));
}

void AppInstallEventLogCollector::AddLogoutEvent() {
  // Don't log in case session is restared.
  if (g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted))
    return;

  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGOUT));
}

void AppInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::SUSPEND));
}

void AppInstallEventLogCollector::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::RESUME));
}

void AppInstallEventLogCollector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  const bool currently_online = GetOnlineState();
  if (currently_online == online_) {
    return;
  }
  online_ = currently_online;

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::CONNECTIVITY_CHANGE);
  event->set_online(online_);
  delegate_->AddForAllPackages(std::move(event));
}

void AppInstallEventLogCollector::OnCloudDpsRequested(
    base::Time time,
    const std::set<std::string>& package_names) {
  for (const std::string& package_name : package_names) {
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(em::AppInstallReportLogEvent::CLOUDDPS_REQUEST);
    SetTimestampFromTime(event.get(), time);
    delegate_->Add(package_name, true /* gather_disk_space_info */,
                   std::move(event));
  }
}

void AppInstallEventLogCollector::OnCloudDpsSucceeded(
    base::Time time,
    const std::set<std::string>& package_names) {
  for (const std::string& package_name : package_names) {
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(em::AppInstallReportLogEvent::CLOUDDPS_RESPONSE);
    SetTimestampFromTime(event.get(), time);
    // Leave clouddps_response untouched.
    delegate_->Add(package_name, true /* gather_disk_space_info */,
                   std::move(event));
  }
}

void AppInstallEventLogCollector::OnCloudDpsFailed(
    base::Time time,
    const std::string& package_name,
    arc::mojom::InstallErrorReason reason) {
  auto event = std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::CLOUDDPS_RESPONSE);
  SetTimestampFromTime(event.get(), time);
  event->set_clouddps_response(static_cast<int>(reason));
  delegate_->Add(package_name, true /* gather_disk_space_info */,
                 std::move(event));
}

void AppInstallEventLogCollector::OnReportDirectInstall(
    base::Time time,
    const std::set<std::string>& package_names) {
  for (const std::string& package_name : package_names) {
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(em::AppInstallReportLogEvent::DIRECT_INSTALL);
    SetTimestampFromTime(event.get(), time);
    delegate_->Add(package_name, true /* gather_disk_space_info */,
                   std::move(event));
  }
}

void AppInstallEventLogCollector::OnReportForceInstallMainLoopFailed(
    base::Time time,
    const std::set<std::string>& package_names) {
  for (const std::string& package_name : package_names) {
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(
        em::AppInstallReportLogEvent::CLOUDDPC_MAIN_LOOP_FAILED);
    SetTimestampFromTime(event.get(), time);
    delegate_->Add(package_name, true /* gather_disk_space_info */,
                   std::move(event));
  }
}

void AppInstallEventLogCollector::OnInstallationStarted(
    const std::string& package_name) {
  if (!pending_packages_.count(package_name)) {
    return;
  }

  auto event = std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::INSTALLATION_STARTED);
  delegate_->Add(package_name, true /* gather_disk_space_info */,
                 std::move(event));
}

void AppInstallEventLogCollector::OnInstallationFinished(
    const std::string& package_name,
    bool success) {
  if (!pending_packages_.count(package_name)) {
    return;
  }

  auto event = std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(
      success ? em::AppInstallReportLogEvent::INSTALLATION_FINISHED
              : em::AppInstallReportLogEvent::INSTALLATION_FAILED);
  delegate_->Add(package_name, true /* gather_disk_space_info */,
                 std::move(event));
}

}  // namespace policy
