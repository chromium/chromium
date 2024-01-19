// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_WEB_APP_INSTALLER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_WEB_APP_INSTALLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace apps {

// The result of a call to WebAppInstaller::InstallApp. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class WebAppInstallResult {
  // Web app was successfully installed.
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

using WebAppInstalledCallback = base::OnceCallback<void(bool success)>;

// WebAppInstaller manages all communication with the web apps system
// (including crosapi where needed) for the purpose of installing web apps.
// TODO(b/315077325): Detach from preloads as a generic way to install web apps
// using Almanac install info.
class WebAppInstaller : public crosapi::WebAppServiceAsh::Observer {
 public:
  explicit WebAppInstaller(Profile* profile);
  ~WebAppInstaller() override;
  WebAppInstaller(const WebAppInstaller&) = delete;
  WebAppInstaller& operator=(const WebAppInstaller&) = delete;

  // Attempts to install each of the `requests` and calls `callback` after all
  // installations have completed. Must only be called if
  // `request.data.app_type_data` holds `WebAppInstallData`.
  struct InstallRequest {
    AppInstallSurface surface;
    AppInstallData data;
  };
  void InstallAllApps(std::vector<InstallRequest> requests,
                      WebAppInstalledCallback callback);

 private:
  // croapi::WebAppServiceAsh::Observer overrides:
  void OnWebAppProviderBridgeConnected() override;
  void OnWebAppServiceAshDestroyed() override;

  // Called when the environment is ready to perform app installation. When
  // lacros is enabled, this means lacros is ready. In ash, this means the
  // WebAppProvider is ready.
  void InstallAllAppsWhenReady();

  void InstallAppImpl(InstallRequest request, WebAppInstalledCallback callback);
  void OnManifestRetrieved(InstallRequest request,
                           WebAppInstalledCallback callback,
                           std::unique_ptr<network::SimpleURLLoader> url_loader,
                           std::unique_ptr<std::string> response);
  void OnAppInstalled(AppInstallSurface surface,
                      WebAppInstalledCallback callback,

                      const webapps::AppId& app_id,
                      webapps::InstallResultCode code);
  void OnAllAppInstallationFinished(const std::vector<bool>& results);

  base::ScopedObservation<crosapi::WebAppServiceAsh,
                          crosapi::WebAppServiceAsh::Observer>
      web_app_service_observer_{this};

  bool lacros_is_connected_;
  std::optional<std::vector<InstallRequest>> requests_for_installation_;
  WebAppInstalledCallback installation_complete_callback_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppInstaller> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_WEB_APP_INSTALLER_H_
