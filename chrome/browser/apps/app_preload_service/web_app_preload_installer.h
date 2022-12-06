// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/install_result_code.h"

class Profile;

namespace apps {

using WebAppPreloadInstalledCallback = base::OnceCallback<void(bool success)>;

// WebAppPreloadInstaller manages all communication with the web apps system
// (including crosapi where needed) for the purpose of installing preloaded web
// apps.
class WebAppPreloadInstaller {
 public:
  explicit WebAppPreloadInstaller(Profile* profile);
  ~WebAppPreloadInstaller();
  WebAppPreloadInstaller(const WebAppPreloadInstaller&) = delete;
  WebAppPreloadInstaller& operator=(const WebAppPreloadInstaller&) = delete;

  // Attempts to install the given web `app`, calling `callback` after
  // installation completes. Must only be called if `app.GetPlatform()` returns
  // `AppType::kWeb`.
  void InstallApp(const PreloadAppDefinition& app,
                  WebAppPreloadInstalledCallback callback);

  // Returns the app ID for the given `app` if it were to be installed as a web
  // app. Does not validate whether the `app` is valid and able to be installed.
  // Must only be called if `app.GetPlatform()` returns `AppType::kWeb`.
  std::string GetAppId(const PreloadAppDefinition& app) const;

 private:
  void InstallAppImpl(PreloadAppDefinition app,
                      WebAppPreloadInstalledCallback callback);
  void OnAppInstalled(WebAppPreloadInstalledCallback callback,
                      const web_app::AppId& app_id,
                      webapps::InstallResultCode code);

  base::raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppPreloadInstaller> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_
