// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include <memory>

#include "ash/public/cpp/accessibility_controller.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/ash/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  if (!extension_registry) {
    // If Lacros is enabled, extensions are running in Lacros. So Ash does not
    // have |extension_registry|.
    return false;
  }

  const extensions::Extension* primary_app =
      extension_registry->GetInstalledExtension(app_id);
  if (!primary_app) {
    return false;
  }

  return extensions::OfflineEnabledInfo::IsOfflineEnabled(primary_app);
}

bool IsLacrosEnabled() {
  return crosapi::browser_util::IsLacrosEnabledInChromeKioskSession() ||
         crosapi::browser_util::IsLacrosEnabledInWebKioskSession();
}

}  // namespace

class AppSessionAsh::LacrosWatcher : public crosapi::BrowserManagerObserver {
 public:
  explicit LacrosWatcher(Profile* profile, const ash::KioskAppId& kiosk_app_id)
      : profile_(profile), kiosk_app_id_(kiosk_app_id) {
    observation_.Observe(crosapi::BrowserManager::Get());
  }

  LacrosWatcher(const LacrosWatcher&) = delete;
  LacrosWatcher& operator=(const LacrosWatcher&) = delete;
  ~LacrosWatcher() override = default;

  // `crosapi::BrowserManagerObserver`:
  void OnStateChanged() override {
    if (!crosapi::BrowserManager::Get()->IsRunningOrWillRun()) {
      LOG(WARNING) << "Lacros crashed, restarting Kiosk session";
      RestartKioskSession();
    }
  }

 private:
  void RestartKioskSession() {
    // Restart the kiosk session. We do not need to create a new
    // `AppSessionAsh`, because ash did not crash in this flow.
    ash::LaunchAppOrDie(profile_, kiosk_app_id_,
                        /*should_start_app_session_ash=*/false);
  }

  const raw_ptr<Profile> profile_;
  const ash::KioskAppId kiosk_app_id_;
  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      observation_{this};
};

AppSessionAsh::AppSessionAsh(Profile* profile,
                             const KioskAppId& kiosk_app_id,
                             const absl::optional<std::string>& app_name)
    : AppSession(profile),
      kiosk_app_id_(kiosk_app_id),
      network_metrics_service_(
          std::make_unique<NetworkConnectivityMetricsService>()),
      periodic_metrics_service_(std::make_unique<PeriodicMetricsService>(
          g_browser_process->local_state())) {
  switch (kiosk_app_id_.type) {
    case KioskAppType::kChromeApp:
      InitForChromeAppKiosk();
      break;
    case KioskAppType::kWebApp:
      InitForWebKiosk(app_name);
      break;
    case KioskAppType::kArcApp:
      NOTREACHED();
  }
  if (IsLacrosEnabled()) {
    lacros_watcher_ = std::make_unique<LacrosWatcher>(profile, kiosk_app_id_);
  }
}

AppSessionAsh::~AppSessionAsh() = default;

void AppSessionAsh::InitForChromeAppKiosk() {
  const std::string& app_id = kiosk_app_id_.app_id.value();
  chromeos::AppSession::InitForChromeAppKiosk(app_id);
  StartFloatingAccessibilityMenu();
  InitKioskAppUpdateService(app_id);
  SetRebootAfterUpdateIfNecessary();

  periodic_metrics_service_->RecordPreviousSessionMetrics();
  periodic_metrics_service_->StartRecordingPeriodicMetrics(
      IsOfflineEnabledForApp(app_id, profile()));
}

void AppSessionAsh::InitForWebKiosk(
    const absl::optional<std::string>& app_name) {
  chromeos::AppSession::InitForWebKiosk(app_name);
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
  if (update_service) {
    update_service->Init(app_id);
  }

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
