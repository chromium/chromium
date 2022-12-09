// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include <memory>

#include "ash/public/cpp/accessibility_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/ash/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"

namespace ash {

namespace {

// Start the floating accessibility menu in ash-chrome if the
// `FloatingAccessibilityMenuEnabled` policy is enabled.
void StartFloatingAccessibilityMenu() {
  auto* accessibility_controller = AccessibilityController::Get();
  if (accessibility_controller)
    accessibility_controller->ShowFloatingMenuIfEnabled();
}

bool IsOfflineEnabledForApp(const std::string& app_id, Profile* profile) {
  const extensions::Extension* primary_app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  return extensions::OfflineEnabledInfo::IsOfflineEnabled(primary_app);
}

}  // namespace

AppSessionAsh::AppSessionAsh(Profile* profile)
    : AppSession(profile),
      network_metrics_service_(
          std::make_unique<NetworkConnectivityMetricsService>()),
      periodic_metrics_service_(std::make_unique<PeriodicMetricsService>(
          g_browser_process->local_state())) {}

AppSessionAsh::~AppSessionAsh() = default;

void AppSessionAsh::Init(const std::string& app_id) {
  chromeos::AppSession::Init(app_id);
  StartFloatingAccessibilityMenu();
  InitKioskAppUpdateService(app_id);
  SetRebootAfterUpdateIfNecessary();

  periodic_metrics_service_->RecordPreviousSessionMetrics();
  periodic_metrics_service_->StartRecordingPeriodicMetrics(
      IsOfflineEnabledForApp(app_id, profile()));
}

void AppSessionAsh::InitForWebKiosk(
    const absl::optional<std::string>& web_app_name) {
  chromeos::AppSession::InitForWebKiosk(web_app_name);
  StartFloatingAccessibilityMenu();

  periodic_metrics_service_->RecordPreviousSessionMetrics();
  // Web apps always support offline mode.
  periodic_metrics_service_->StartRecordingPeriodicMetrics(
      /*is_offline_enabled=*/true);
}

void AppSessionAsh::ShuttingDown() {
  network_metrics_service_.reset();
}

void AppSessionAsh::InitKioskAppUpdateService(const std::string& app_id) {
  // Set the app_id for the current instance of KioskAppUpdateService.
  auto* update_service = KioskAppUpdateServiceFactory::GetForProfile(profile());
  DCHECK(update_service);
  if (update_service)
    update_service->Init(app_id);

  // Start to monitor external update from usb stick.
  KioskAppManager::Get()->MonitorKioskExternalUpdate();
}

void AppSessionAsh::SetRebootAfterUpdateIfNecessary() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kRebootAfterUpdate, true);
    KioskModeIdleAppNameNotification::Initialize();
  }
}

}  // namespace ash
