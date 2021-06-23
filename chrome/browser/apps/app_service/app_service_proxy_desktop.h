// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace web_app {
class WebApps;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
class WebAppsPublisherHost;
#endif
}  // namespace web_app

namespace apps {

class ExtensionApps;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class FakeLacrosWebAppsHost;
#endif

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
                 apps::mojom::UninstallSource uninstall_source,
                 gfx::NativeWindow parent_window) override;
  void FlushMojoCallsForTesting() override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::WebAppsPublisherHost* WebAppsPublisherHostForTesting();
#endif

 private:
  // apps::AppServiceProxyBase overrides:
  void Initialize() override;
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;

  // KeyedService overrides:
  void Shutdown() override;

  std::unique_ptr<web_app::WebApps> web_apps_;
  std::unique_ptr<ExtensionApps> extension_apps_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeLacrosWebAppsHost> fake_lacros_web_apps_host_;
  std::unique_ptr<web_app::WebAppsPublisherHost> web_apps_publisher_host_;
#endif

  base::WeakPtrFactory<AppServiceProxy> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
