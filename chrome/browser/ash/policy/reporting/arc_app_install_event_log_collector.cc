// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
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

void SetTimestampFromTime(em::AppInstallReportLogEvent* event,
                          base::Time time) {
  event->set_timestamp((time - base::Time::UnixEpoch()).InMicroseconds());
}

}  // namespace

ArcAppInstallEventLogCollector::ArcAppInstallEventLogCollector(
    Delegate* delegate,
    Profile* profile,
    const std::set<std::string>& pending_packages)
    : InstallEventLogCollectorBase(profile),
      delegate_(delegate),
      pending_packages_(pending_packages) {
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

ArcAppInstallEventLogCollector::~ArcAppInstallEventLogCollector() {
  ArcAppListPrefs* const app_prefs = ArcAppListPrefs::Get(profile_);
  if (app_prefs) {
    app_prefs->RemoveObserver(this);
  }
  arc::ArcPolicyBridge* const policy_bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  if (policy_bridge) {
    policy_bridge->RemoveObserver(this);
  }
}

void ArcAppInstallEventLogCollector::OnPendingPackagesChanged(
    const std::set<std::string>& pending_packages) {
  pending_packages_ = pending_packages;
}

void ArcAppInstallEventLogCollector::OnLoginInternal() {
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGIN);
  event->set_online(online_);
  delegate_->AddForAllPackages(std::move(event));
}

void ArcAppInstallEventLogCollector::OnLogoutInternal() {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGOUT));
}

void ArcAppInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::SUSPEND));
}

void ArcAppInstallEventLogCollector::SuspendDone(
    base::TimeDelta sleep_duration) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::RESUME));
}

void ArcAppInstallEventLogCollector::OnConnectionStateChanged(
    network::mojom::ConnectionType type) {
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::CONNECTIVITY_CHANGE);
  event->set_online(online_);
  delegate_->AddForAllPackages(std::move(event));
}

void ArcAppInstallEventLogCollector::OnInstallationStarted(
    const std::string& package_name) {
  if (!pending_packages_.count(package_name)) {
    return;
  }

  auto event = std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::INSTALLATION_STARTED);
  delegate_->Add(package_name, true /* gather_disk_space_info */,
                 std::move(event));
}

void ArcAppInstallEventLogCollector::OnInstallationFinished(
    const std::string& package_name,
    bool success,
    bool is_launchable_app) {
  if (!pending_packages_.count(package_name)) {
    return;
  }

  auto event = std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(
      success ? em::AppInstallReportLogEvent::INSTALLATION_FINISHED
              : em::AppInstallReportLogEvent::INSTALLATION_FAILED);
  delegate_->UpdatePolicySuccessRate(package_name, success);
  delegate_->Add(package_name, true /* gather_disk_space_info */,
                 std::move(event));
}

void ArcAppInstallEventLogCollector::OnPlayStoreLocalPolicySet(
    base::Time time,
    const std::set<std::string>& package_names) {
  for (const std::string& package_name : package_names) {
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(
        em::AppInstallReportLogEvent::PLAYSTORE_LOCAL_POLICY_SET);
    SetTimestampFromTime(event.get(), time);
    delegate_->Add(package_name, true /* gather_disk_space_info */,
                   std::move(event));
  }
}

}  // namespace policy
