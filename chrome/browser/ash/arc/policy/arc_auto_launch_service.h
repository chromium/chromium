// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class ArcAppLauncher;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Manages auto-launching configured ARC applications on user login for
// managed users.
// Only the first app listed in the AutoLaunchAndroidApps policy is launched.
class ArcAutoLaunchService : public KeyedService,
                             public ArcAppListPrefs::Observer {
 public:
  // How long to wait for the configured app to be installed and launched
  // before giving up, so that it does not show up unexpectedly late in the
  // session.
  static constexpr base::TimeDelta kLaunchTimeout = base::Minutes(5);

  static ArcAutoLaunchService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcAutoLaunchService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcAutoLaunchService(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);

  ArcAutoLaunchService(const ArcAutoLaunchService&) = delete;
  ArcAutoLaunchService& operator=(const ArcAutoLaunchService&) = delete;

  ~ArcAutoLaunchService() override;

  // KeyedService:
  void Shutdown() override;

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnArcAppListPrefsDestroyed() override;

  static void EnsureFactoryBuilt();

 private:
  // Starts launching `package_name`. Does nothing if it is empty.
  void StartLaunch(const std::string& package_name);
  // Launches the app if a launchable activity of `package_name_` is
  // registered.
  void MaybeLaunchApp();
  // Calls MaybeLaunchApp() once the current batch of app updates is processed,
  // if `app_info` belongs to `package_name_`.
  void MaybeLaunchAppAfterUpdate(const ArcAppListPrefs::AppInfo& app_info);
  // Stops launching `package_name_`.
  void StopLaunch();
  // Returns true if the app has been launched.
  bool HasLaunchedApp() const;
  // Gives up launching the app if it has not been launched yet.
  void OnTimeout();
  // Applies changes to the AutoLaunchAndroidApps policy.
  void OnPolicyChanged();

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // The package to launch. Empty if there is none.
  std::string package_name_;
  // Launches the app once it is ready.
  std::unique_ptr<ArcAppLauncher> app_launcher_;
  base::OneShotTimer timeout_timer_;
  // Observes ArcAppListPrefs while waiting for the app to be installed.
  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_app_list_prefs_observation_{this};

  base::WeakPtrFactory<ArcAutoLaunchService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_AUTO_LAUNCH_SERVICE_H_
