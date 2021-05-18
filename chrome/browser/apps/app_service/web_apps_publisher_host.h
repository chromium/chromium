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
#include "chrome/browser/apps/app_service/app_web_contents_data.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ContentSettingsPattern;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
class WebApp;
class WebAppProvider;
class WebAppRegistrar;
}  // namespace web_app

namespace apps {

// This WebAppsPublisherHost observes AppRegistrar on Lacros, and calls
// WebAppsCrosapi to inform the Ash browser of the current set of web apps.
class WebAppsPublisherHost : public crosapi::mojom::AppController,
                             public web_app::AppRegistrarObserver,
                             public content_settings::Observer,
                             public MediaCaptureDevicesDispatcher::Observer,
                             public AppWebContentsData::Client {
 public:
  explicit WebAppsPublisherHost(Profile* profile);
  WebAppsPublisherHost(const WebAppsPublisherHost&) = delete;
  WebAppsPublisherHost& operator=(const WebAppsPublisherHost&) = delete;
  ~WebAppsPublisherHost() override;

  void Init();

  Profile* profile() { return profile_; }
  web_app::WebAppRegistrar& registrar() const;

  void SetPublisherForTesting(crosapi::mojom::AppPublisher* publisher);

 private:
  void OnReady();

  // crosapi::mojom::AppController:
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;

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

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // TODO(crbug.com/1194709): Add more overrides, guided by WebAppsChromeOs.

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // AppWebContentsData::Client:
  void OnWebContentsDestroyed(content::WebContents* contents) override;

  const web_app::WebApp* GetWebApp(const web_app::AppId& app_id) const;
  apps::mojom::AppPtr Convert(const web_app::WebApp* web_app,
                              apps::mojom::Readiness readiness);
  void Publish(apps::mojom::AppPtr app);

  void ModifyCapabilityAccess(const std::string& app_id,
                              absl::optional<bool> accessing_camera,
                              absl::optional<bool> accessing_microphone);

  Profile* const profile_;
  web_app::WebAppProvider* const provider_;
  crosapi::mojom::AppPublisher* remote_publisher_ = nullptr;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  mojo::Receiver<crosapi::mojom::AppController> receiver_{this};

  base::ScopedObservation<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      media_dispatcher_{this};

  MediaRequests media_requests_;

  base::WeakPtrFactory<WebAppsPublisherHost> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
