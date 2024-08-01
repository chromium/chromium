// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_update_observer.h"

#include "base/syslog_logging.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace ash {

WebKioskAppUpdateObserver::WebKioskAppUpdateObserver(
    Profile* profile,
    const AccountId& account_id)
    : account_id_(account_id) {
  app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service_);
  web_app_provider_ = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(web_app_provider_);
  app_registry_observation_.Observe(&app_service_->AppRegistryCache());
}

WebKioskAppUpdateObserver::~WebKioskAppUpdateObserver() = default;

void WebKioskAppUpdateObserver::OnAppUpdate(const apps::AppUpdate& update) {
  // There can be only one Kiosk-installed web app per Kiosk session. So that
  // we treat any update coming from Kiosk-installed as the one configured in
  // policy.
  if (update.AppType() != apps::AppType::kWeb ||
      update.InstallReason() != apps::InstallReason::kKiosk ||
      update.Readiness() != apps::Readiness::kReady) {
    return;
  }

  // If any of the following changes, we update the Kiosk Apps menu:
  // Name: title of the app
  // PublisherId: start URL of the app
  // IconKey: icon of the app
  if (!update.NameChanged() && !update.PublisherIdChanged() &&
      !update.IconKeyChanged()) {
    return;
  }

  if (web_app_provider_->registrar_unsafe().IsPlaceholderApp(
          update.AppId(), web_app::WebAppManagement::Type::kKiosk)) {
    SYSLOG(INFO) << "Ignoring web app update of placeholder app";
    return;
  }

  SYSLOG(INFO) << "Kiosk web app update triggered";
  UpdateWebAppFromAppService(update.AppId(), update.IconKeyChanged());
}

void WebKioskAppUpdateObserver::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observation_.Reset();
}

void WebKioskAppUpdateObserver::UpdateWebAppFromAppService(
    const std::string& app_id,
    bool icon_updated) {
  // `apps::AppUpdate` passed to `OnAppUpdate()` only contains updated info. To
  // get all info we have to use `AppRegistryCache::ForOneApp()`.
  app_service_->AppRegistryCache().ForOneApp(
      app_id, [this, &icon_updated](const apps::AppUpdate& app_info) {
        GURL start_url = GURL(app_info.PublisherId());

        if (icon_updated && app_info.IconKey()) {
          app_service_->LoadIconWithIconEffects(
              // Remove web app icon effects for Kiosk apps menu.
              app_info.AppId(), apps::IconEffects::kNone,
              apps::IconType::kUncompressed, kWebKioskIconSize,
              /*allow_placeholder_icon=*/true,
              base::BindOnce(&WebKioskAppUpdateObserver::OnAppServiceIconLoaded,
                             weak_ptr_factory_.GetWeakPtr(), app_info.Name(),
                             std::move(start_url)));
          return;
        }

        WebKioskAppManager::Get()->UpdateAppByAccountId(
            account_id_, app_info.Name(), start_url, web_app::IconBitmaps());
      });
}

void WebKioskAppUpdateObserver::OnAppServiceIconLoaded(
    std::string title,
    GURL start_url,
    apps::IconValuePtr icon) {
  web_app::IconBitmaps icon_bitmaps;
  if (icon->uncompressed.bitmap()) {
    icon_bitmaps.any[kWebKioskIconSize] = *icon->uncompressed.bitmap();
  }
  WebKioskAppManager::Get()->UpdateAppByAccountId(account_id_, title, start_url,
                                                  icon_bitmaps);
}

}  // namespace ash
