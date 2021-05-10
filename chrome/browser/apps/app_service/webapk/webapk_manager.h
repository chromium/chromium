// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_

#include "chrome/browser/web_applications/components/app_registrar.h"
#include "components/arc/mojom/intent_helper.mojom-forward.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace ash {
class ApkWebAppService;
}

namespace apps {

class AppServiceProxyBase;
class WebApkInstallQueue;

class WebApkManager : public apps::AppRegistryCache::Observer {
 public:
  explicit WebApkManager(Profile* profile);
  ~WebApkManager() override;

  WebApkManager(const WebApkManager&) = delete;
  WebApkManager& operator=(const WebApkManager&) = delete;

  // AppRegistryCache::Observer overrides:
  void OnAppUpdate(const AppUpdate& update) override;
  void OnAppTypeInitialized(apps::mojom::AppType type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  apps::WebApkInstallQueue* GetInstallQueueForTest();

 private:
  bool IsAppEligibleForWebApk(const apps::AppUpdate& app);
  void QueueInstall(const apps::AppUpdate& update);

  Profile* profile_;
  apps::AppServiceProxyBase* proxy_;
  ash::ApkWebAppService* apk_service_;
  web_app::AppRegistrar& web_app_registrar_;

  bool initialized_;

  std::unique_ptr<apps::WebApkInstallQueue> install_queue_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_
