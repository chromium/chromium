// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/menu.h"

class StandaloneBrowserPublisherTest;

namespace apps {
struct AppLaunchParams;

// An app publisher (in the App Service sense) for extension-based apps [e.g.
// packaged v2 apps] published by Lacros.
//
// The App Service is responsible for integrating various app platforms [ARC,
// Lacros, Crostini, etc.] with system UI: shelf, launcher, etc. The App Service
// expects publishers to implement a uniform API, roughly translating to:
// GetLoadedApps, LaunchApp, GetIconForApp, etc.
//
// This class implements this functionality for extension based apps running in
// the Lacros context. Aforementioned API is implemented using crosapi to
// communicate between this class and Lacros.
//
// The current implementation of this class relies on the assumption that Lacros
// is almost always running in the background. This is enforced via
// ScopedKeepAlive. We would like to eventually remove this assumption. This
// requires caching a copy of installed apps in this class.
class StandaloneBrowserExtensionApps : public KeyedService,
                                       public AppPublisher,
                                       public crosapi::mojom::AppPublisher,
                                       public ash::LoginState::Observer {
 public:
  StandaloneBrowserExtensionApps(AppServiceProxy* proxy, AppType app_type);
  ~StandaloneBrowserExtensionApps() override;

  StandaloneBrowserExtensionApps(const StandaloneBrowserExtensionApps&) =
      delete;
  StandaloneBrowserExtensionApps& operator=(
      const StandaloneBrowserExtensionApps&) = delete;

  // Register the host (for Chrome Apps or Extensions) from Lacros to allow the
  // matching publisher to publish to the App Service in Ash.
  void RegisterCrosapiHost(
      mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver);

 private:
  friend class StandaloneBrowserPublisherTest;
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           StandaloneBrowserExtensionAppsNotUpdated);
  FRIEND_TEST_ALL_PREFIXES(StandaloneBrowserPublisherTest,
                           StandaloneBrowserExtensionAppsUpdated);

  // apps::AppPublisher overrides.
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void Uninstall(const std::string& app_id,
                 UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;
  void SetWindowMode(const std::string& app_id,
                     WindowMode window_mode) override;
  void StopApp(const std::string& app_id) override;
  void UpdateAppSize(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // crosapi::mojom::AppPublisher overrides.
  void OnApps(std::vector<AppPtr> deltas) override;
  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override;
  void OnCapabilityAccesses(std::vector<CapabilityAccessPtr> deltas) override;

  // ash::LoginState::Observer
  void LoggedInStateChanged() override;

  // Called when the crosapi termination is terminated [e.g. Lacros is closed].
  // The ordering of these two disconnect methods is non-deterministic. When
  // either is called we close both connections since this class requires both
  // to be functional.
  void OnReceiverDisconnected();
  void OnControllerDisconnected();

  const AppType app_type_;

  // This class stores a copy of the latest apps received for each app_id, which
  // haven't been published to AppRegistryCache yet. When the crosapi is bound
  // or changed from disconnect to bound, we need to publish all apps in this
  // cache to AppRegistryCache.
  //
  // The Lacros sender of OnApps events always sends full objects, not deltas.
  // Thus, this class can simply keep the latest copy, without doing any
  // merging.
  std::map<std::string, apps::AppPtr> app_cache_;

  bool should_notify_initialized_ = true;

  // Receives chrome app publisher events from Lacros.
  mojo::Receiver<crosapi::mojom::AppPublisher> receiver_{this};

  // Used to send chrome app publisher actions to Lacros.
  mojo::Remote<crosapi::mojom::AppController> controller_;

  std::unique_ptr<crosapi::BrowserManagerScopedKeepAlive> keep_alive_;

  base::ScopedObservation<ash::LoginState, ash::LoginState::Observer>
      login_observation_{this};

  base::WeakPtrFactory<StandaloneBrowserExtensionApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_H_
