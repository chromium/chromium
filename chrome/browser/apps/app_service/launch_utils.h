// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/arc/mojom/app.mojom.h"
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

std::string GetAppIdForWebContents(content::WebContents* web_contents);

bool IsInstalledApp(Profile* profile, const std::string& app_id);

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id);

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
    apps::mojom::AppLaunchSource source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container);

apps::AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::AppLaunchSource source,
    int64_t display_id,
    apps::mojom::LaunchContainer fallback_container,
    apps::mojom::IntentPtr&& intent);

apps::mojom::AppLaunchSource GetAppLaunchSource(
    apps::mojom::LaunchSource launch_source);

// Returns event flag for |container| and |disposition|. If |prefer_container|
// is true, |disposition| will be ignored. Otherwise, |container| is ignored and
// an event flag based on |disposition| will be returned.
int GetEventFlags(apps::mojom::LaunchContainer container,
                  WindowOpenDisposition disposition,
                  bool prefer_container);

// Returns the browser's session id for restoration if |web_contents| is valid
// for a system web app, or for a web app not opened in tab. Otherwise, returns
// an invalid session id.
int GetSessionIdForRestoreFromWebContents(
    const content::WebContents* web_contents);

// Helper to create apps::mojom::WindowInfoPtr using |display_id|, which is the
// id of the display from which the app is launched.
apps::mojom::WindowInfoPtr MakeWindowInfo(int64_t display_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Helper to convert apps::mojom::WindowInfoPtr to arc::mojom::WindowInfoPtr.
arc::mojom::WindowInfoPtr MakeArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTILS_H_
