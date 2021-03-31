// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/web_apps.h"

class Profile;

namespace apps {

// Singleton (per Profile) proxy and cache of an App Service's apps in Chrome
// browser.
//
// See components/services/app_service/README.md.
class AppServiceProxy : public AppServiceProxyBase {
 public:
  explicit AppServiceProxy(Profile* profile);
  AppServiceProxy(const AppServiceProxy&) = delete;
  AppServiceProxy& operator=(const AppServiceProxy&) = delete;
  ~AppServiceProxy() override;

  // apps::AppServiceProxyBase overrides:
  void Uninstall(const std::string& app_id,
                 gfx::NativeWindow parent_window) override;
  void FlushMojoCallsForTesting() override;

 private:
  // apps::AppServiceProxyBase overrides:
  void Initialize() override;
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;

  std::unique_ptr<WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> extension_apps_;

  base::WeakPtrFactory<AppServiceProxy> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
