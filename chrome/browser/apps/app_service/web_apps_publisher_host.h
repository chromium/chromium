// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace web_app {
class WebApp;
class WebAppProvider;
class WebAppRegistrar;
}  // namespace web_app

namespace apps {

// This WebAppsPublisherHost observes AppRegistrar on Lacros, and calls
// WebAppsCrosapi to inform the Ash browser of the current set of web apps.
class WebAppsPublisherHost : public web_app::AppRegistrarObserver {
 public:
  explicit WebAppsPublisherHost(Profile* profile);
  WebAppsPublisherHost(const WebAppsPublisherHost&) = delete;
  WebAppsPublisherHost& operator=(const WebAppsPublisherHost&) = delete;
  ~WebAppsPublisherHost() override;

  web_app::WebAppRegistrar& registrar() const;

  crosapi::mojom::AppPublisher* GetPublisher() const;

  static void SetPublisherForTesting(crosapi::mojom::AppPublisher* publisher);

 private:
  void OnReady();

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppManifestUpdated(const web_app::AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;
  void OnWebAppLocallyInstalledStateChanged(const web_app::AppId& app_id,
                                            bool is_locally_installed) override;
  void OnWebAppLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;

  // TODO(crbug.com/1194709): inherit from content_settings::Observer and
  // override:
  // - OnContentSettingChanged

  // TODO(crbug.com/1194709): Add more overrides, guided by WebAppsChromeOs.

  const web_app::WebApp* GetWebApp(const web_app::AppId& app_id) const;
  apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                              apps::mojom::Readiness readiness);
  void Publish(apps::mojom::AppPtr app);

  static crosapi::mojom::AppPublisher* publisher_for_testing_;

  Profile* const profile_;
  web_app::WebAppProvider* const provider_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  base::ScopedObservation<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observation_{this};

  base::WeakPtrFactory<WebAppsPublisherHost> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
