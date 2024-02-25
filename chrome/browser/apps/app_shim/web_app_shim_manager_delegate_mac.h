// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_WEB_APP_SHIM_MANAGER_DELEGATE_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_WEB_APP_SHIM_MANAGER_DELEGATE_MAC_H_

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

#include "base/functional/callback.h"

namespace content {
class WebContents;
}

namespace apps {
struct AppLaunchParams;
}

namespace web_app {

using BrowserAppLauncherForTesting =
    base::RepeatingCallback<content::WebContents*(
        const apps::AppLaunchParams& params)>;

// Test helper that hooking calls to BrowserAppLauncher::LaunchAppWithParams
void SetBrowserAppLauncherForTesting(
    const BrowserAppLauncherForTesting& launcher);

class WebAppShimManagerDelegate : public apps::AppShimManager::Delegate {
 public:
  // AppShimManager::Delegate:
  WebAppShimManagerDelegate(
      std::unique_ptr<apps::AppShimManager::Delegate> fallback_delegate);
  ~WebAppShimManagerDelegate() override;
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
                  apps::ShimLaunchedCallback launched_callback,
                  apps::ShimTerminatedCallback terminated_callback) override;
  bool HasNonBookmarkAppWindowsOpen() override;
  std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
  GetAppShortcutsMenuItemInfos(Profile* profile,
                               const webapps::AppId& app_id) override;

 private:
  // Return true if |fallback_delegate_| should be used instead of |this|.
  bool UseFallback(Profile* profile, const webapps::AppId& app_id) const;

  // This is the delegate used by extension-based applications. When they are
  // removed, then this may be deleted.
  std::unique_ptr<apps::AppShimManager::Delegate> fallback_delegate_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_APPS_APP_SHIM_WEB_APP_SHIM_MANAGER_DELEGATE_MAC_H_
