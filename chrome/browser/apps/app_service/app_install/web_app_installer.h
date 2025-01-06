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
// for the purpose of installing web apps.
// TODO(b/315077325): Detach from preloads as a generic way to install web apps
// using Almanac install info.
class WebAppInstaller {
 public:
  explicit WebAppInstaller(Profile* profile);
  ~WebAppInstaller();
  WebAppInstaller(const WebAppInstaller&) = delete;
  WebAppInstaller& operator=(const WebAppInstaller&) = delete;

  // Must only be called if `data.app_type_data` holds `WebAppInstallData`.
  void InstallApp(AppInstallSurface surface,
                  AppInstallData data,
                  WebAppInstalledCallback callback);

 private:
  void OnManifestRetrieved(AppInstallSurface surface,
                           AppInstallData data,
                           WebAppInstalledCallback callback,
                           std::unique_ptr<network::SimpleURLLoader> url_loader,
                           std::unique_ptr<std::string> response);

  void OnAppInstalled(AppInstallSurface surface,
                      WebAppInstalledCallback callback,
                      const webapps::AppId& app_id,
                      webapps::InstallResultCode code);

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppInstaller> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_WEB_APP_INSTALLER_H_
