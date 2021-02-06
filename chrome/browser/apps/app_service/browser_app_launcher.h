// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Browser;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}

namespace web_app {
class WebAppLaunchManager;
}  // namespace web_app

namespace apps {

// BrowserAppLauncher receives app launch requests and forwards them to
// extensions or WebAppLaunchManager, based on the app type.
//
// TODO(crbug.com/1061843): Remove BrowserAppLauncher and merge the interfaces
// to AppServiceProxy when publishers(ExtensionApps and WebApps) can run on
// Chrome.
class BrowserAppLauncher {
 public:
  explicit BrowserAppLauncher(Profile* profile);
  ~BrowserAppLauncher();

  BrowserAppLauncher(const BrowserAppLauncher&) = delete;
  BrowserAppLauncher& operator=(const BrowserAppLauncher&) = delete;

  // Launches an app for the given |app_id| in a way specified by |params|.
  content::WebContents* LaunchAppWithParams(AppLaunchParams&& params);

  // Attempts to open |app_id| in a new window or tab. Open an empty browser
  // window if unsuccessful. The user's preferred launch container for the app
  // (standalone window or browser tab) is used. |callback| will be called with
  // the container type used to open the app, kLaunchContainerNone if an empty
  // browser window was opened.
  void LaunchAppWithCallback(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback);

 private:
  Profile* const profile_;
  web_app::WebAppLaunchManager web_app_launch_manager_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
