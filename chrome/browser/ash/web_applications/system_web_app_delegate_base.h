// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_DELEGATE_BASE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_DELEGATE_BASE_H_

#include <map>
#include <string>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace web_app {
class WebAppProvider;

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;

// Use #if defined to avoid compiler error on unused function.
#if BUILDFLAG(IS_CHROMEOS_ASH)
// A convenience method to help creating an OriginTrialsMap. Note, we only
// support simple cases for chrome:// and chrome-untrusted:// URLs. We don't
// support complex cases such as about:blank (which inherits origins from the
// embedding frame).
url::Origin GetOrigin(const char* url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A class for configuring SWAs for all the out-of-application stuff. For
// example, window decorations and initial size. Clients will add a subclass for
// their application, overriding GetWebAppInfo(), and other methods as needed.
class SystemWebAppDelegateBase : public web_app::SystemWebAppDelegate {
 public:
  SystemWebAppDelegateBase(
      const SystemAppType type,
      const std::string& internal_name,
      const GURL& install_url,
      Profile* profile,
      const OriginTrialsMap& origin_trials_map = OriginTrialsMap());

  SystemWebAppDelegateBase(const SystemWebAppDelegateBase& other) = delete;

  SystemWebAppDelegateBase& operator=(const SystemWebAppDelegateBase&) = delete;

  ~SystemWebAppDelegateBase() override;

  // If specified, the apps in |uninstall_and_replace| will have their data
  // migrated to this System App.
  std::vector<web_app::AppId> GetAppIdsToUninstallAndReplace() const override;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  gfx::Size GetMinimumWindowSize() const override;

  // If set, we allow only a single window for this app.
  bool ShouldReuseExistingWindow() const override;

  // If true, adds a "New Window" option to App's shelf context menu.
  // ShouldReuseExistingWindow() should return false at the same time.
  bool ShouldShowNewWindowMenuOption() const override;

  // If true, when the app is launched through the File Handling Web API, we
  // will include the file's directory in window.launchQueue as the first value.
  bool ShouldIncludeLaunchDirectory() const override;

  // Resource Ids for additional search terms.
  std::vector<int> GetAdditionalSearchTerms() const override;

  // If false, this app will be hidden from the Chrome OS app launcher.
  bool ShouldShowInLauncher() const override;

  // If false, this app will be hidden from the Chrome OS search.
  bool ShouldShowInSearch() const override;

  // If true, navigations (e.g. Omnibox URL, anchor link) to this app
  // will open in the app's window instead of the navigation's context (e.g.
  // browser tab).
  bool ShouldCaptureNavigations() const override;

  // If false, the app will non-resizeable.
  bool ShouldAllowResize() const override;

  // If false, the surface of app will can be non-maximizable.
  bool ShouldAllowMaximize() const override;

  // If true, the App's window will have a tab-strip.
  bool ShouldHaveTabStrip() const override;

  // If false, the app will not have the reload button in minimal ui
  // mode.
  bool ShouldHaveReloadButtonInMinimalUi() const override;

  // If true, allows the app to close the window through scripts, for example
  // using `window.close()`.
  bool ShouldAllowScriptsToCloseWindows() const override;

  // Setup information to drive a background task.
  absl::optional<web_app::SystemAppBackgroundTaskInfo> GetTimerInfo()
      const override;

  // Default window bounds of the application.
  gfx::Rect GetDefaultBounds(Browser* browser) const override;

  // If false, the application will not be installed.
  bool IsAppEnabled() const override;

  // If true, GetTabMenuModel() is called to provide the tab menu model.
  bool HasCustomTabMenuModel() const override;

  // Optional custom tab menu model.
  std::unique_ptr<ui::SimpleMenuModel> GetTabMenuModel(
      ui::SimpleMenuModel::Delegate* delegate) const override;

  // Returns whether the specified Tab Context Menu shortcut should be shown.
  bool ShouldShowTabContextMenuShortcut(Profile* profile,
                                        int command_id) const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Whether the browser should show the Terminal System App select new tab
  // button in the toolbar.
  bool HasTitlebarTerminalSelectNewTabButton() const override;
#endif

  // Control the launch of an SWA. The default takes into account single vs.
  // multiple windows, make sure multiple windows don't open directly above
  // each other, and a few other niceties. Overriding this will require some
  // knowledge of browser window and launch internals, so hopefully you'll never
  // have to roll your own here.

  // If true is returned, browser is expected to be not-null, and app launch
  // will continue. If false is returned, it's assumed that this method has
  // cleaned up after itself, and launch is aborted.
  Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SYSTEM_WEB_APP_DELEGATE_BASE_H_
