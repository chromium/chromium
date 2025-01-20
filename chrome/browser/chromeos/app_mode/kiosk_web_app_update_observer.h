// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_UPDATE_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace chromeos {

// Observes web app update from App Service and updates information via a
// callback. It persists through the whole Kiosk session so that app updates
// during the session can also be handled. This class must be created after
// WebAppProvider is ready. It can be achieved by waiting for
// `apps::AppType::kWeb` to be ready in `apps::AppServiceProxy`.
class KioskWebAppUpdateObserver : public apps::AppRegistryCache::Observer {
 public:
  // Updates app by title, start_url and icon_bitmaps.
  using WebAppUpdateCallback =
      base::RepeatingCallback<void(const AccountId&,
                                   const std::string&,
                                   const GURL&,
                                   const web_app::IconBitmaps&)>;

  KioskWebAppUpdateObserver(Profile* profile,
                            const AccountId& account_id,
                            int requested_icon_size,
                            WebAppUpdateCallback callback);

  KioskWebAppUpdateObserver(const KioskWebAppUpdateObserver&) = delete;
  KioskWebAppUpdateObserver& operator=(const KioskWebAppUpdateObserver&) =
      delete;
  ~KioskWebAppUpdateObserver() override;

 private:
  // apps::AppRegistryCache::Observer
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void UpdateWebAppFromAppService(const std::string& app_id, bool icon_updated);

  void OnAppServiceIconLoaded(std::string title,
                              GURL start_url,
                              apps::IconValuePtr icon);

  const AccountId account_id_;
  const int requested_icon_size_;
  const raw_ptr<apps::AppServiceProxy> app_service_;
  const raw_ptr<const web_app::WebAppProvider> web_app_provider_;
  const WebAppUpdateCallback web_app_update_callback_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_observation_{this};

  base::WeakPtrFactory<KioskWebAppUpdateObserver> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_UPDATE_OBSERVER_H_
