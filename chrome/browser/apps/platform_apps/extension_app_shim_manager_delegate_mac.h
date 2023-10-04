// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

namespace apps {

class ExtensionAppShimManagerDelegate : public AppShimManager::Delegate {
 public:
  // AppShimManager::Delegate:
  ExtensionAppShimManagerDelegate();
  ~ExtensionAppShimManagerDelegate() override;
  bool ShowAppWindows(Profile* profile, const webapps::AppId& app_id) override;
  void CloseAppWindows(Profile* profile, const webapps::AppId& app_id) override;
  bool AppIsInstalled(Profile* profile, const webapps::AppId& app_id) override;
  bool AppCanCreateHost(Profile* profile,
                        const webapps::AppId& app_id) override;
  bool AppUsesRemoteCocoa(Profile* profile,
                          const webapps::AppId& app_id) override;
  bool AppIsMultiProfile(Profile* profile,
                         const webapps::AppId& app_id) override;
  void EnableExtension(Profile* profile,
                       const std::string& extension_id,
                       base::OnceCallback<void()> callback) override;
  void LaunchApp(
      Profile* profile,
      const webapps::AppId& app_id,
      const std::vector<base::FilePath>& files,
      const std::vector<GURL>& urls,
      const GURL& override_url,
      chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
      base::OnceClosure launch_finished_callback) override;
  void LaunchShim(Profile* profile,
                  const webapps::AppId& app_id,
                  web_app::LaunchShimUpdateBehavior update_behavior,
                  web_app::ShimLaunchMode launch_mode,
                  ShimLaunchedCallback launched_callback,
                  ShimTerminatedCallback terminated_callback) override;
  bool HasNonBookmarkAppWindowsOpen() override;
  std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
  GetAppShortcutsMenuItemInfos(Profile* profile,
                               const webapps::AppId& app_id) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_EXTENSION_APP_SHIM_MANAGER_DELEGATE_MAC_H_
