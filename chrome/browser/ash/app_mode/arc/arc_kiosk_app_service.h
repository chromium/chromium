// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_

#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/arc/kiosk/arc_kiosk_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class ArcKioskAppManager;

// Keeps track of ARC session state and auto-launches kiosk app when it's ready.
// App is started when the following conditions are satisfied:
// 1. App id is registered in ArcAppListPrefs and set to "ready" state.
// 2. Got empty policy compliance report from Android
// 3. App is not yet started
// Also, the app is stopped when one of above conditions changes.
class ArcKioskAppService : public KeyedService,
                           public ArcAppListPrefs::Observer,
                           public KioskAppManagerObserver,
                           public arc::ArcKioskBridge::Delegate,
                           public ArcKioskAppLauncher::Delegate,
                           public ArcAppIcon::Observer,
                           public arc::ArcSessionManagerObserver,
                           public arc::ArcPolicyBridge::Observer,
                           public KioskAppLauncher {
 public:
  static ArcKioskAppService* Create(Profile* profile);
  static ArcKioskAppService* Get(content::BrowserContext* context);

  ArcKioskAppService(const ArcKioskAppService&) = delete;
  ArcKioskAppService& operator=(const ArcKioskAppService&) = delete;

  void SetNetworkDelegate(NetworkDelegate* network_delegate);

  // KeyedService overrides
  void Shutdown() override;

  // ArcAppListPrefs::Observer overrides
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnPackageListInitialRefreshed() override;

  // KioskAppManagerObserver overrides
  void OnKioskAppsSettingsChanged() override;

  // ArcKioskBridge::Delegate overrides
  void OnMaintenanceSessionCreated() override;
  void OnMaintenanceSessionFinished() override;

  // ArcKioskAppLauncher::Delegate overrides
  void OnAppWindowLaunched() override;

  // ArcAppIcon::Observer overrides
  void OnIconUpdated(ArcAppIcon* icon) override;

  // ArcSessionManagerObserver overrides
  void OnArcSessionRestarting() override;
  void OnArcSessionStopped(arc::ArcStopReason reason) override;

  // ArcPolicyBridge::Observer overrides
  void OnComplianceReportReceived(
      const base::Value* compliance_report) override;

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void RestartLauncher() override;
  void LaunchApp() override;

  ArcKioskAppLauncher* GetLauncherForTesting() { return app_launcher_.get(); }

 private:
  explicit ArcKioskAppService(Profile* profile);
  ~ArcKioskAppService() override;

  std::string GetAppId();
  // Called when app should be started or stopped.
  void PreconditionsChanged();
  // Updates local cache with proper name and icon.
  void RequestNameAndIconUpdate();
  // Triggered when app is closed to reset launcher.
  void ResetAppLauncher();

  Profile* const profile_;
  bool maintenance_session_running_ = false;
  base::OneShotTimer maintenance_timeout_timer_;
  ArcKioskAppManager* app_manager_;
  std::string app_id_;
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_;
  std::unique_ptr<ArcAppIcon> app_icon_;
  int32_t task_id_ = -1;
  KioskAppLauncher::ObserverList observers_;

  // This contains the list of apps that must be installed for the device to be
  // policy-compliant according to the policy report. Even if an app has already
  // finished installing, it could still remain in this list for some time.
  // ArcKioskAppService may only start apps which are not in this list anymore,
  // because it's only assured that kiosk policies have been applied (e.g.
  // permission policies) when the app is not in this list anymore.
  base::flat_set<std::string> pending_policy_app_installs_;
  bool compliance_report_received_ = false;
  // Keeps track whether the app is already launched
  std::unique_ptr<ArcKioskAppLauncher> app_launcher_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_
