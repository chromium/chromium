// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace apps {

// The result of a call to WebAppPreloadInstaller::InstallApp. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class WebAppPreloadResult {
  // Preloaded app was successfully installed.
  kSuccess = 0,
  // Web app manifest URL was invalid and could not be downloaded.
  kInvalidManifestUrl = 1,
  // A network error occurred while downloading manifest.
  kManifestNetworkError = 2,
  // The request to download the manifest contents completed with an error
  // status code.
  kManifestResponseError = 3,
  // The request to download the manifest contents completed successfully, but
  // with an empty manifest.
  kManifestResponseEmpty = 4,
  // The web app installation command completed with an error.
  kWebAppInstallError = 5,
  kMaxValue = kWebAppInstallError
};

using WebAppPreloadInstalledCallback = base::OnceCallback<void(bool success)>;

// WebAppPreloadInstaller manages all communication with the web apps system
// (including crosapi where needed) for the purpose of installing preloaded web
// apps.
class WebAppPreloadInstaller : public crosapi::WebAppServiceAsh::Observer {
 public:
  explicit WebAppPreloadInstaller(Profile* profile);
  ~WebAppPreloadInstaller() override;
  WebAppPreloadInstaller(const WebAppPreloadInstaller&) = delete;
  WebAppPreloadInstaller& operator=(const WebAppPreloadInstaller&) = delete;

  // Attempts to install each of the `apps` and calls `callback` after all
  // installations have completed. Must only be called if `app.GetPlatform()`
  // returns `AppType::kWeb`.
  void InstallAllApps(std::vector<PreloadAppDefinition> apps,
                      WebAppPreloadInstalledCallback callback);

  // Returns the app ID for the given `app` if it were to be installed as a web
  // app. Does not validate whether the `app` is valid and able to be installed.
  // Must only be called if `app.GetPlatform()` returns `AppType::kWeb`.
  std::string GetAppId(const PreloadAppDefinition& app) const;

 private:
  // croapi::WebAppServiceAsh::Observer overrides:
  void OnWebAppProviderBridgeConnected() override;

  // Called when the environment is ready to perform app installation. When
  // lacros is enabled, this means lacros is ready. In ash, this means the
  // WebAppProvider is ready.
  void InstallAllAppsWhenReady();

  void InstallAppImpl(PreloadAppDefinition app,
                      WebAppPreloadInstalledCallback callback);
  void OnManifestRetrieved(PreloadAppDefinition app,
                           WebAppPreloadInstalledCallback callback,
                           std::unique_ptr<network::SimpleURLLoader> url_loader,
                           std::unique_ptr<std::string> response);
  void OnAppInstalled(WebAppPreloadInstalledCallback callback,
                      const webapps::AppId& app_id,
                      webapps::InstallResultCode code);
  void OnAllAppInstallationFinished(const std::vector<bool>& results);

  base::ScopedObservation<crosapi::WebAppServiceAsh,
                          crosapi::WebAppServiceAsh::Observer>
      web_app_service_observer_{this};

  bool lacros_is_connected_;
  absl::optional<std::vector<PreloadAppDefinition>> apps_for_installation_;
  WebAppPreloadInstalledCallback installation_complete_callback_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppPreloadInstaller> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_WEB_APP_PRELOAD_INSTALLER_H_
