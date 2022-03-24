// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;
class GURL;
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

#if !BUILDFLAG(IS_CHROMEOS)
  // Launches an app for the given `app_id` in a way specified by `params`.
  //
  // This interface is deprecated, please use
  // AppServiceProxy::LaunchAppWithParams() in the future.
  // TODO(crbug.com/1244506): Remove this interface in non-chrome OS platform.
  content::WebContents* LaunchAppWithParams(AppLaunchParams params);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Launches an app for the given `app_id` in a way specified by `params`. This
  // interface should only be used in testing code where reqired a sync launch
  // for browser apps. Please try to use AppServiceProxy::LaunchAppWithParams()
  // interface where possible. This interface is deprecated in the production
  // code, using this interface might cause behaviour difference between the
  // production code and testing code.
  // TODO(crbug.com/1289100): Remove this interface if all usages are removed.
  content::WebContents* LaunchAppWithParamsForTesting(AppLaunchParams params);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Launches Play Store with Extensions. ARC and Extensions share the same app
  // id for Play Store, and App Service only registered the play store ARC app
  // (see blocklist in
  // chrome/browser/apps/app_service/publishers/extension_apps_chromeos.cc
  // for more details). Therefore we need this interface to launch Play Store
  // with Extensions when ARC is not enabled.
  void LaunchPlayStoreWithExtensions();
#endif

  // Attempts to open `app_id` in a new window or tab. Open an empty browser
  // window if unsuccessful. The user's preferred launch container for the app
  // (standalone window or browser tab) is used. `callback` will be called with
  // the container type used to open the app, kLaunchContainerNone if an empty
  // browser window was opened. `callback`'s `browser` will be nullptr if the
  // navigation failed.
  // `url_handler_launch_url` is the launch URL when a PWA should be launched
  // as the URL handler. It's null if it's not a URL handler launch.
  // `protocol_handler_launch_url` is the protocol URL when a PWA is launched
  // as a protocol handler. It's null if it's not a protocol handler launch.
  // `launch_files` is a list of files to be passed to the PWA when it is
  // launched as a file handler, or empty if it's not a file handling launch.
  void LaunchAppWithCallback(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url,
      const std::vector<base::FilePath>& launch_files,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback);

 private:
  const raw_ptr<Profile> profile_;
  web_app::WebAppLaunchManager web_app_launch_manager_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
