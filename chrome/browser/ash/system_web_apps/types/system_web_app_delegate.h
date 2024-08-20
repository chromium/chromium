// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"
#include "url/origin.h"

class Browser;
class Profile;

namespace apps {
struct AppLaunchParams;
}  // namespace apps

namespace gfx {
class Rect;
}  // namespace gfx

namespace web_app {
struct WebAppInstallInfo;
class WebAppProvider;
}  // namespace web_app

namespace ash {

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;

// Use #if defined to avoid compiler error on unused function.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// A convenience method to create OriginTrialsMap. Note, we only support simple
// cases for chrome:// and chrome-untrusted:// URLs. We don't support complex
// cases such as about:blank (which inherits origins from the embedding frame).
url::Origin GetOrigin(const char* url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A class for configuring SWAs for all the out-of-application stuff. For
// example, window decorations and initial size. Clients will add a subclass for
// their application, overriding GetWebAppInfo(), and other methods as needed.
class SystemWebAppDelegate {
 public:
  // When installing via a WebAppInstallInfo, the url is never loaded. It's
  // needed only for various legacy reasons, maps for tracking state, and
  // generating the AppId and things of that nature.
  SystemWebAppDelegate(
      SystemWebAppType type,
      const std::string& internal_name,
      const GURL& install_url,
      Profile* profile,
      const OriginTrialsMap& origin_trials_map = OriginTrialsMap());

  SystemWebAppDelegate(const SystemWebAppDelegate& other) = delete;
  SystemWebAppDelegate& operator=(const SystemWebAppDelegate& other) = delete;
  virtual ~SystemWebAppDelegate();

  SystemWebAppType GetType() const { return type_; }

  // A developer-friendly name for, among other things, reporting metrics
  // and interacting with tast tests. It should follow PascalCase
  // convention, and have a corresponding entry in
  // WebAppSystemAppInternalName histogram suffixes. The internal name
  // shouldn't be changed afterwards.
  const std::string& GetInternalName() const { return internal_name_; }

  // The URL that the System App will be installed from.
  const GURL& GetInstallUrl() const { return install_url_; }

  // Returns a WebAppInstallInfo struct to complete installation.
  virtual std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const = 0;

  // Returns a vector of AppIDs. Each app_id (a string id) may correspond to any
  // ChromeOS app: ChromeApp, WebApp, Arc++ etc. The apps specified will have
  // their data migrated to this System App.
  virtual std::vector<std::string> GetAppIdsToUninstallAndReplace() const;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  virtual gfx::Size GetMinimumWindowSize() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Decides whether to launch the app at the given url in an existing app
  // window (returned by the function) or a new one (nullptr). By default, an
  // existing app window is reused independent of the url.
  //
  // This is implemented in
  // chrome/browser/ui/ash/system_web_apps/system_web_app_delegate_ui_impl.cc.
  virtual Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If true, adds a "New Window" option to App's shelf context menu.
  // NOTE: Combining this with a GetWindowForLaunch function that allows window
  // reuse can lead to an unintuitive UX.
  virtual bool ShouldShowNewWindowMenuOption() const;

  // Called when the app is launched with `params`. If the returned value is
  // non-empty, it will be passed to the page as a FileSystemDirectoryHandle
  // pre-pended to the `launchParams` list.
  virtual base::FilePath GetLaunchDirectory(
      const apps::AppLaunchParams& params) const;

  // Map from origin to enabled origin trial names for this app. For example,
  // "chrome://sample-web-app/" to ["Frobulate"]. If set, we will enable the
  // given origin trials when the corresponding origin is loaded in the app.
  const OriginTrialsMap& GetEnabledOriginTrials() const {
    return origin_trials_map_;
  }

  // Resource Ids for additional search terms.
  virtual std::vector<int> GetAdditionalSearchTerms() const;

  // If false, this app will be hidden from the Chrome OS app launcher. If true,
  // the app must have a launcher position defined in the GetDefault() function
  // in //chrome/browser/ash/extensions/default_app_order.cc, which should match
  // the order in go/default-apps.
  virtual bool ShouldShowInLauncher() const;

  // If false, this app will be hidden from both the Chrome OS search and shelf.
  // If true, this app will be shown in both the ChromeOS search and shelf.
  virtual bool ShouldShowInSearchAndShelf() const;

  // If true, in Ash browser, navigations (e.g. Omnibox URL, anchor link) to
  // this app will open in the app's window instead of the navigation's context
  // (e.g. browser tab).
  //
  // This feature isn't applicable to Lacros browser. If you need navigations in
  // Lacros to launch the app, use crosapi URL handler by adding the app's URL
  // to `ChromeWebUIControllerFactory::GetListOfAcceptableURLs()`.
  virtual bool ShouldCaptureNavigations() const;

  // If false, the app will non-resizeable.
  virtual bool ShouldAllowResize() const;

  // If false, the surface of app will can be non-maximizable.
  virtual bool ShouldAllowMaximize() const;

  // If false, the surface of the app can not enter fullscreen.
  virtual bool ShouldAllowFullscreen() const;

  // If true, the App's window will have a tab-strip.
  virtual bool ShouldHaveTabStrip() const;

  // If true, the new-tab button on the tab-strip will be hidden. Only
  // applicable if the app's window has a tab-strip.
  virtual bool ShouldHideNewTabButton() const;

  // If false, the app will not have the reload button in minimal ui
  // mode.
  virtual bool ShouldHaveReloadButtonInMinimalUi() const;

  // If true, allows the app to close the window through scripts, for example
  // using `window.close()`.
  virtual bool ShouldAllowScriptsToCloseWindows() const;

  // If true, allows app to show up in file-open intent and picking surfaces.
  virtual bool ShouldHandleFileOpenIntents() const;

  // Setup information to drive a background task.
  virtual std::optional<SystemWebAppBackgroundTaskInfo> GetTimerInfo() const;

  // Default window bounds of the application.
  virtual gfx::Rect GetDefaultBounds(Browser* browser) const;

  // If false, the application will not be installed.
  virtual bool IsAppEnabled() const;

  // If true, GetTabMenuModel() is called to provide the tab menu model.
  virtual bool HasCustomTabMenuModel() const;

  // Optional custom tab menu model.
  virtual std::unique_ptr<ui::SimpleMenuModel> GetTabMenuModel(
      ui::SimpleMenuModel::Delegate* delegate) const;

  // Returns whether the specified Tab Context Menu shortcut should be shown.
  virtual bool ShouldShowTabContextMenuShortcut(Profile* profile,
                                                int command_id) const;

  // Returns whether the override URL specified in AppLaunchParams should be
  // used when performing a full restore.
  virtual bool ShouldRestoreOverrideUrl() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Control the launch of an SWA. The default takes into account single vs.
  // multiple windows, make sure multiple windows don't open directly above
  // each other, and a few other niceties. Overriding this will require some
  // knowledge of browser window and launch internals, so hopefully you'll never
  // have to roll your own here.
  //
  // If a browser is returned, app launch will continue. If false is returned,
  // it's assumed that this method has cleaned up after itself, and launch is
  // aborted.
  //
  // This is implemented in
  // chrome/browser/ui/ash/system_web_apps/system_web_app_delegate_ui_impl.cc.
  virtual Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Whether |url| which is outside the normal Navigation Scope should be
  // considered part of this System App.
  virtual bool IsUrlInSystemAppScope(const GURL& url) const;

  // Whether theme color should be inferred from ChromeOS system theme. If
  // true, theme_color is the first available from:
  //   1. System theme color (if kJelly is on).
  //   2. Manifest color (if defined).
  //   3. Default color.
  virtual bool UseSystemThemeColor() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns whether theme changes should be animated.
  virtual bool ShouldAnimateThemeChanges() const;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1308961): Migrate to use PWA pinned home tab when ready.
  // Returns whether the specified tab should be pinned.
  virtual bool ShouldPinTab(GURL url) const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  Profile* profile() const { return profile_; }

  // These should all be private. See
  // https://google.github.io/styleguide/cppguide.html#Access_Control
  SystemWebAppType type_;
  std::string internal_name_;
  GURL install_url_;
  raw_ptr<Profile> profile_;
  OriginTrialsMap origin_trials_map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TYPES_SYSTEM_WEB_APP_DELEGATE_H_
