// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_CHROMEOS_H_

#include <map>
#include <set>

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/publishers/borealis_apps.h"
#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/apps/app_service/publishers/lacros_web_apps.h"
#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_chromeos.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace apps {

class LacrosApps;
class UninstallDialog;

struct PauseData {
  int hours = 0;
  int minutes = 0;
  bool should_show_pause_dialog = false;
};

// Singleton (per Profile) proxy and cache of an App Service's apps on Chrome
// OS.
//
// See components/services/app_service/README.md.
class AppServiceProxyChromeOs : public AppServiceProxyBase {
 public:
  using OnPauseDialogClosedCallback = base::OnceCallback<void()>;

  explicit AppServiceProxyChromeOs(Profile* profile);
  AppServiceProxyChromeOs(const AppServiceProxyChromeOs&) = delete;
  AppServiceProxyChromeOs& operator=(const AppServiceProxyChromeOs&) = delete;
  ~AppServiceProxyChromeOs() override;

  apps::InstanceRegistry& InstanceRegistry();

  // apps::AppServiceProxyBase overrides:
  void Uninstall(const std::string& app_id,
                 gfx::NativeWindow parent_window) override;

  // Pauses apps. |pause_data|'s key is the app_id. |pause_data|'s PauseData
  // is the time limit setting for the app, which is shown in the pause app
  // dialog. AppService sets the paused status directly. If the app is running,
  // AppService shows the pause app dialog. Otherwise, AppService applies the
  // paused app icon effect directly.
  void PauseApps(const std::map<std::string, PauseData>& pause_data);

  // Unpauses the apps from the paused status. AppService sets the paused status
  // as false directly and removes the paused app icon effect.
  void UnpauseApps(const std::set<std::string>& app_ids);

  // Set whether resize lock is enabled for the app identified by |app_id|.
  void SetResizeLocked(const std::string& app_id,
                       apps::mojom::OptionalBool locked);

  // Sets |extension_apps_| and |web_apps_| to observe the ARC apps to set the
  // badge on the equivalent Chrome app's icon, when ARC is available.
  void SetArcIsRegistered();

  // apps::AppServiceProxyBase overrides:
  void FlushMojoCallsForTesting() override;

  void ReInitializeCrostiniForTesting(Profile* profile);
  void SetDialogCreatedCallbackForTesting(base::OnceClosure callback);
  void UninstallForTesting(const std::string& app_id,
                           gfx::NativeWindow parent_window,
                           base::OnceClosure callback);

 private:
  using UninstallDialogs = std::set<std::unique_ptr<apps::UninstallDialog>,
                                    base::UniquePtrComparator>;

  void Initialize() override;

  // KeyedService overrides.
  void Shutdown() override;

  static void CreateBlockDialog(const std::string& app_name,
                                const gfx::ImageSkia& image,
                                Profile* profile);

  static void CreatePauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_name,
                                const gfx::ImageSkia& image,
                                const PauseData& pause_data,
                                OnPauseDialogClosedCallback pause_callback);

  void UninstallImpl(const std::string& app_id,
                     gfx::NativeWindow parent_window,
                     base::OnceClosure callback);

  // Invoked when the uninstall dialog is closed. The app for the given
  // |app_type| and |app_id| will be uninstalled directly if |uninstall| is
  // true. |clear_site_data| is available for bookmark apps only. If true, any
  // site data associated with the app will be removed. |report_abuse| is
  // available for Chrome Apps only. If true, the app will be reported for abuse
  // to the Web Store. |uninstall_dialog| will be removed from
  // |uninstall_dialogs_|.
  void OnUninstallDialogClosed(apps::mojom::AppType app_type,
                               const std::string& app_id,
                               bool uninstall,
                               bool clear_site_data,
                               bool report_abuse,
                               UninstallDialog* uninstall_dialog);

  // apps::AppServiceProxyBase overrides:
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;

  // Loads the icon for the app block dialog or the app pause dialog.
  void LoadIconForDialog(const apps::AppUpdate& update,
                         apps::mojom::Publisher::LoadIconCallback callback);

  // Callback invoked when the icon is loaded for the block app dialog.
  void OnLoadIconForBlockDialog(const std::string& app_name,
                                apps::mojom::IconValuePtr icon_value);

  // Callback invoked when the icon is loaded for the pause app dialog.
  void OnLoadIconForPauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_id,
                                const std::string& app_name,
                                const PauseData& pause_data,
                                apps::mojom::IconValuePtr icon_value);

  // Invoked when the user clicks the 'OK' button of the pause app dialog.
  // AppService stops the running app and applies the paused app icon effect.
  void OnPauseDialogClosed(apps::mojom::AppType app_type,
                           const std::string& app_id);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;

  std::unique_ptr<BuiltInChromeOsApps> built_in_chrome_os_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionAppsChromeOs> extension_apps_;
  std::unique_ptr<PluginVmApps> plugin_vm_apps_;
  std::unique_ptr<LacrosApps> lacros_apps_;
  std::unique_ptr<WebAppsChromeOs> web_apps_;
  std::unique_ptr<BorealisApps> borealis_apps_;
  std::unique_ptr<LacrosWebApps> lacros_web_apps_;

  bool arc_is_registered_ = false;

  apps::InstanceRegistry instance_registry_;

  // When PauseApps is called, the app is added to |pending_pause_requests|.
  // When the user clicks the OK from the pause app dialog, the pause status is
  // updated in AppRegistryCache by the publisher, then the app is removed from
  // |pending_pause_requests|. If the app status is paused in AppRegistryCache
  // or pending_pause_requests, the app can't be launched.
  PausedApps pending_pause_requests_;

  UninstallDialogs uninstall_dialogs_;

  base::WeakPtrFactory<AppServiceProxyChromeOs> weak_ptr_factory_{this};
};

class ScopedOmitBuiltInAppsForTesting {
 public:
  ScopedOmitBuiltInAppsForTesting();
  ScopedOmitBuiltInAppsForTesting(const ScopedOmitBuiltInAppsForTesting&) =
      delete;
  ScopedOmitBuiltInAppsForTesting& operator=(
      const ScopedOmitBuiltInAppsForTesting&) = delete;
  ~ScopedOmitBuiltInAppsForTesting();

 private:
  const bool previous_omit_built_in_apps_for_testing_;
};

class ScopedOmitPluginVmAppsForTesting {
 public:
  ScopedOmitPluginVmAppsForTesting();
  ScopedOmitPluginVmAppsForTesting(const ScopedOmitPluginVmAppsForTesting&) =
      delete;
  ScopedOmitPluginVmAppsForTesting& operator=(
      const ScopedOmitPluginVmAppsForTesting&) = delete;
  ~ScopedOmitPluginVmAppsForTesting();

 private:
  const bool previous_omit_plugin_vm_apps_for_testing_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_CHROMEOS_H_
