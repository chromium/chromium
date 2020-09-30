// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {
class WebApp;
class WebAppLaunchManager;
class WebAppProvider;
class WebAppRegistrar;
}  // namespace web_app

namespace apps {

// An app publisher (in the App Service sense) of Web Apps.
class WebAppsBase : public apps::PublisherBase,
                    public web_app::AppRegistrarObserver,
                    public content_settings::Observer {
 public:
  WebAppsBase(const mojo::Remote<apps::mojom::AppService>& app_service,
              Profile* profile);
  WebAppsBase(const WebAppsBase&) = delete;
  WebAppsBase& operator=(const WebAppsBase&) = delete;
  ~WebAppsBase() override;

  virtual void Shutdown();

 protected:
  const web_app::WebApp* GetWebApp(const web_app::AppId& app_id) const;

  // web_app::AppRegistrarObserver:
  void OnWebAppUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;

  apps::mojom::AppPtr ConvertImpl(const web_app::WebApp* web_app,
                                  apps::mojom::Readiness readiness);

  IconEffects GetIconEffects(const web_app::WebApp* web_app);

  content::WebContents* LaunchAppWithIntentImpl(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      int64_t display_id);

  const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers() const {
    return subscribers_;
  }

  Profile* profile() const { return profile_; }
  web_app::WebAppProvider* provider() const { return provider_; }

  base::WeakPtr<WebAppsBase> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  apps_util::IncrementingIconKeyFactory& icon_key_factory() {
    return icon_key_factory_;
  }

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // Can return nullptr in tests.
  const web_app::WebAppRegistrar* GetRegistrar() const;

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          apps::mojom::LaunchContainer container,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppManifestUpdated(const web_app::AppId& app_id,
                               base::StringPiece old_name) override;
  void OnAppRegistrarDestroyed() override;
  void OnWebAppLocallyInstalledStateChanged(const web_app::AppId& app_id,
                                            bool is_locally_installed) override;

  void SetShowInFields(apps::mojom::AppPtr& app,
                       const web_app::WebApp* web_app);
  void PopulatePermissions(const web_app::WebApp* web_app,
                           std::vector<mojom::PermissionPtr>* target);
  virtual apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                                      apps::mojom::Readiness readiness) = 0;
  void ConvertWebApps(apps::mojom::Readiness readiness,
                      std::vector<apps::mojom::AppPtr>* apps_out);
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  virtual bool Accepts(const std::string& app_id) = 0;

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observer_{this};

  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};

  web_app::WebAppProvider* provider_ = nullptr;

  std::unique_ptr<web_app::WebAppLaunchManager> web_app_launch_manager_;

  // app_service_ is owned by the object that owns this object.
  apps::mojom::AppService* app_service_;

  base::WeakPtrFactory<WebAppsBase> weak_ptr_factory_{this};
};

void PopulateIntentFilters(const web_app::WebApp& web_app,
                           std::vector<mojom::IntentFilterPtr>& target);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_BASE_H_
