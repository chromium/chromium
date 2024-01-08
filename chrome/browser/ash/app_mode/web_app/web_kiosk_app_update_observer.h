// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_UPDATE_OBSERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace ash {

// Observes web app update from App Service and updates information stored in
// `WebKioskAppManager`. It persists through the whole Kiosk session so that app
// updates during the session can also be handled.
// This class must be created after WebAppProvider is ready. It can be achieved
// by waiting for `apps::AppType::kWeb` to be ready in `apps::AppServiceProxy`.
class WebKioskAppUpdateObserver : public apps::AppRegistryCache::Observer {
 public:
  WebKioskAppUpdateObserver(Profile* profile, const AccountId& account_id);

  WebKioskAppUpdateObserver(const WebKioskAppUpdateObserver&) = delete;
  WebKioskAppUpdateObserver& operator=(const WebKioskAppUpdateObserver&) =
      delete;
  ~WebKioskAppUpdateObserver() override;

 private:
  // apps::AppRegistryCache::Observer
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Updates app info in `WebKioskAppManager`.
  void UpdateWebAppFromAppService(const std::string& app_id, bool icon_updated);

  void OnAppServiceIconLoaded(std::string title,
                              GURL start_url,
                              apps::IconValuePtr icon);

  AccountId account_id_;

  raw_ptr<apps::AppServiceProxy> app_service_;
  raw_ptr<web_app::WebAppProvider> web_app_provider_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_observation_{this};

  base::WeakPtrFactory<WebKioskAppUpdateObserver> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_UPDATE_OBSERVER_H_
