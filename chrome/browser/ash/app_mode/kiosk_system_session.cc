// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_system_session.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"
#include "chrome/browser/ash/app_mode/crash_recovery_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"

namespace ash {

namespace {

// Start the floating accessibility menu in ash-chrome if the
// `FloatingAccessibilityMenuEnabled` policy is enabled.
void StartFloatingAccessibilityMenu() {
  auto* accessibility_controller = AccessibilityController::Get();
  if (accessibility_controller) {
    accessibility_controller->ShowFloatingMenuIfEnabled();
  }
}

bool IsOfflineEnabledForApp(const std::string& app_id, Profile* profile) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  CHECK(extension_registry);

  const extensions::Extension* primary_app =
      extension_registry->GetInstalledExtension(app_id);
  if (!primary_app) {
    return false;
  }

  return extensions::OfflineEnabledInfo::IsOfflineEnabled(primary_app);
}

}  // namespace

KioskSystemSession::KioskSystemSession(
    Profile* profile,
    const KioskAppId& kiosk_app_id,
    const std::optional<std::string>& app_name)
    : profile_(profile),
      browser_session_(profile),
      kiosk_app_id_(kiosk_app_id),
      network_metrics_service_(
          std::make_unique<NetworkConnectivityMetricsService>()),
      periodic_metrics_service_(std::make_unique<PeriodicMetricsService>(
          g_browser_process->local_state())),
      device_weekly_scheduled_suspend_controller_(
          std::make_unique<DeviceWeeklyScheduledSuspendController>(
              g_browser_process->local_state())),
      network_state_observer_(profile->GetPrefs()) {
  switch (kiosk_app_id_.type) {
    case KioskAppType::kChromeApp:
      InitForChromeAppKiosk();
      break;
    case KioskAppType::kWebApp:
      InitForWebKiosk(app_name);
      break;
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/361017777): call InitForIwaKiosk or reus existing init.
      NOTIMPLEMENTED();
      break;
  }
}

KioskSystemSession::~KioskSystemSession() = default;

void KioskSystemSession::InitForChromeAppKiosk() {
  const std::string& app_id = kiosk_app_id_.app_id.value();
  browser_session_.InitForChromeAppKiosk(app_id);
  StartFloatingAccessibilityMenu();
  InitKioskAppUpdateService(app_id);
  SetRebootAfterUpdateIfNecessary();

  periodic_metrics_service_->RecordPreviousSessionMetrics();
  periodic_metrics_service_->StartRecordingPeriodicMetrics(
      IsOfflineEnabledForApp(app_id, profile()));
}

void KioskSystemSession::InitForWebKiosk(
    const std::optional<std::string>& app_name) {
  browser_session_.InitForWebKiosk(app_name);
  StartFloatingAccessibilityMenu();

  periodic_metrics_service_->RecordPreviousSessionMetrics();
  // Web apps always support offline mode.
  periodic_metrics_service_->StartRecordingPeriodicMetrics(
      /*is_offline_enabled=*/true);
}

void KioskSystemSession::ShuttingDown() {
  network_metrics_service_.reset();
}

void KioskSystemSession::InitKioskAppUpdateService(const std::string& app_id) {
  // Set the app_id for the current instance of KioskAppUpdateService.
  auto* update_service = KioskAppUpdateServiceFactory::GetForProfile(profile());
  DCHECK(update_service);
  if (update_service) {
    update_service->Init(app_id);
  }

  // Start to monitor external update from usb stick.
  KioskChromeAppManager::Get()->MonitorKioskExternalUpdate();
}

void KioskSystemSession::SetRebootAfterUpdateIfNecessary() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(::prefs::kRebootAfterUpdate, true);
    KioskModeIdleAppNameNotification::Initialize();
  }
}

void KioskSystemSession::OnGuestAdded(
    content::WebContents* guest_web_contents) {
  browser_session_.OnGuestAdded(guest_web_contents);
}

void KioskSystemSession::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled, false);
}

Profile* KioskSystemSession::profile() const {
  CHECK(profile_);
  return profile_;
}

bool KioskSystemSession::is_shutting_down() const {
  return browser_session_.is_shutting_down();
}

Browser* KioskSystemSession::GetSettingsBrowserForTesting() {
  return browser_session_.GetSettingsBrowserForTesting();  // IN-TEST
}

void KioskSystemSession::SetOnHandleBrowserCallbackForTesting(
    base::RepeatingCallback<void(bool)> callback) {
  browser_session_.SetOnHandleBrowserCallbackForTesting(callback);  // IN-TEST
}

}  // namespace ash
