// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/app.mojom-forward.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace apps {

LaunchContainer ConvertWindowModeToAppLaunchContainer(WindowMode window_mode);

// Converts file arguments to an app on |command_line| into base::FilePaths.
std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line);

// When a command line launch has an unknown app id, we open a browser with only
// the new tab page.
Browser* CreateBrowserWithNewTabPage(Profile* profile);

// Helper to create AppLaunchParams using event flags that allows user to
// override the user-configured container using modifier keys. |display_id| is
// the id of the display from which the app is launched.
AppLaunchParams CreateAppIdLaunchParamsWithEventFlags(
    const std::string& app_id,
    int event_flags,
    LaunchSource source,
    int64_t display_id,
    LaunchContainer fallback_container);

AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource source,
    int64_t display_id,
    LaunchContainer fallback_container,
    IntentPtr&& intent,
    Profile* profile);

extensions::AppLaunchSource GetAppLaunchSource(LaunchSource launch_source);

// Returns event flag for |disposition|. If |prefer_container|
// is true, |disposition| will be ignored. Otherwise, an event flag based on
// |disposition| will be returned.
int GetEventFlags(WindowOpenDisposition disposition, bool prefer_container);

// Returns the browser's session id for restoration if |web_contents| is valid
// for a system web app, or for a web app not opened in tab. Otherwise, returns
// an invalid session id.
int GetSessionIdForRestoreFromWebContents(
    const content::WebContents* web_contents);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Helper to convert apps::mojom::WindowInfoPtr to arc::mojom::WindowInfoPtr.
arc::mojom::WindowInfoPtr MakeArcWindowInfo(WindowInfoPtr window_info);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// Helper to convert apps::AppLaunchParams to crosapi::mojom::LaunchParams.
// This is needed because we cannot use traits to convert Intent at the moment,
// After that is done, this can be moved to the mojom type traits.
crosapi::mojom::LaunchParamsPtr ConvertLaunchParamsToCrosapi(
    const AppLaunchParams& params,
    Profile* profile);

// Helper to convert crosapi::mojom::LaunchParams to apps::AppLaunchParams.
// This is needed because we cannot use traits to convert Intent at the moment,
// After that is done, this can be moved to the mojom type traits.
AppLaunchParams ConvertCrosapiToLaunchParams(
    const crosapi::mojom::LaunchParamsPtr& crosapi_params,
    Profile* profile);

crosapi::mojom::LaunchParamsPtr CreateCrosapiLaunchParamsWithEventFlags(
    AppServiceProxy* proxy,
    const std::string& app_id,
    int event_flags,
    LaunchSource launch_source,
    int64_t display_id);

// Container for holding possible app IDs that can launch a PWA for a given URL.
struct AppIdsToLaunchForUrl {
  AppIdsToLaunchForUrl();
  AppIdsToLaunchForUrl(AppIdsToLaunchForUrl&&);
  ~AppIdsToLaunchForUrl();

  // Apps that can handle a given URL.
  std::vector<std::string> candidates;
  // The users preference for an app to handle a given URL.
  std::optional<std::string> preferred;
};

// Takes a `url` and returns a vector of app IDs and the users preferred choice
// of app that can launch a PWA for the given `url`.
AppIdsToLaunchForUrl FindAppIdsToLaunchForUrl(AppServiceProxy* proxy,
                                              const GURL& url);

// Checks to see if any apps handle `url` and is selected as the users
// preference. If so, launches the preferred app, otherwise opens `url` in a
// browser tab.
void MaybeLaunchPreferredAppForUrl(Profile* profile,
                                   const GURL& url,
                                   LaunchSource launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Launches `url` in a suitable installed app, or in the browser if no app is
// installed. If one app is installed which can handle `url`, it will always be
// opened. If multiple apps are installed, any app which is preferred by the
// user will be opened.
void LaunchUrlInInstalledAppOrBrowser(Profile* profile,
                                      const GURL& url,
                                      LaunchSource launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
