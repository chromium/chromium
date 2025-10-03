// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"

class Profile;

namespace content {
class WebContents;
}

namespace apps {

// BrowserAppLauncher receives app launch requests and forwards them to
// extensions or LaunchWebAppCommand, based on the app type.
//
// TODO(crbug.com/40122594): Remove BrowserAppLauncher and merge the interfaces
// to AppServiceProxy when publishers(ExtensionApps and WebApps) can run on
// Chrome.
class BrowserAppLauncher {
 public:
  explicit BrowserAppLauncher(Profile* profile);
  ~BrowserAppLauncher();

  BrowserAppLauncher(const BrowserAppLauncher&) = delete;
  BrowserAppLauncher& operator=(const BrowserAppLauncher&) = delete;

  // Launches an app for the given `app_id` in a way specified by `params`. This
  // interface should only be used in testing code where reqired a sync launch
  // for browser apps. Please try to use AppServiceProxy::LaunchAppWithParams()
  // interface where possible.
  // Note: This code will block until the launch occurs.
  //
  // Deprecated. Prefer `LaunchAppWithParams()` or `LaunchAppWithIntent`.
  // This interface is deprecated in production code, as using it might cause
  // behaviour difference between the production code and test code.
  // TODO(crbug.com/40211799): Remove this interface if all usages are removed.
  content::WebContents* LaunchAppWithParamsForTesting(AppLaunchParams params);

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_LAUNCHER_H_
