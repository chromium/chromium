// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_
#define CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_

#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace crosapi {

// Created in lacros-chrome. Allows ash-chrome to modify web app state in
// lacros-chrome.
class WebAppProviderBridgeLacros : public mojom::WebAppProviderBridge {
 public:
  WebAppProviderBridgeLacros();
  WebAppProviderBridgeLacros(const WebAppProviderBridgeLacros&) = delete;
  WebAppProviderBridgeLacros& operator=(const WebAppProviderBridgeLacros&) =
      delete;
  ~WebAppProviderBridgeLacros() override;

  // mojom::WebAppProviderBridge overrides:
  void WebAppInstalledInArc(mojom::ArcWebAppInstallInfoPtr info,
                            WebAppInstalledInArcCallback callback) override;
  void WebAppUninstalledInArc(const std::string& app_id,
                              WebAppUninstalledInArcCallback callback) override;
  void GetWebApkCreationParams(
      const std::string& app_id,
      GetWebApkCreationParamsCallback callback) override;
  void InstallMicrosoft365(InstallMicrosoft365Callback callback) override;
  void ScheduleNavigateAndTriggerInstallDialog(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated) override;
  void GetSubAppIds(const webapps::AppId& app_id,
                    GetSubAppIdsCallback callback) override;
  void GetSubAppToParentMap(GetSubAppToParentMapCallback callback) override;
  void InstallWebAppFromVerifiedManifest(
      mojom::WebAppVerifiedManifestInstallInfoPtr preload_install_info,
      InstallWebAppFromVerifiedManifestCallback callback) override;
  void LaunchIsolatedWebAppInstaller(
      const base::FilePath& bundle_path) override;

 private:
  static void WebAppInstalledInArcImpl(
      mojom::ArcWebAppInstallInfoPtr arc_install_info,
      WebAppInstalledInArcCallback callback,
      Profile* profile);
  static void WebAppUninstalledInArcImpl(
      const std::string& app_id,
      WebAppUninstalledInArcCallback callback,
      Profile* profile);
  static void GetWebApkCreationParamsImpl(
      const std::string& app_id,
      GetWebApkCreationParamsCallback callback,
      Profile* profile);
  static void InstallMicrosoft365Impl(InstallMicrosoft365Callback callback,
                                      Profile* profile);
  static void ScheduleNavigateAndTriggerInstallDialogImpl(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      Profile* profile);
  static void GetSubAppIdsImpl(const webapps::AppId& app_id,
                               GetSubAppIdsCallback callback,
                               Profile* profile);
  static void GetSubAppToParentMapImpl(GetSubAppToParentMapCallback callback,
                                       Profile* profile);
  static void InstallWebAppFromVerifiedManifestImpl(
      mojom::WebAppVerifiedManifestInstallInfoPtr preload_install_info,
      InstallWebAppFromVerifiedManifestCallback callback,
      Profile* profile);
  static void LaunchIsolatedWebAppInstallerImpl(
      const base::FilePath& bundle_path,
      Profile* profile);

  mojo::Receiver<mojom::WebAppProviderBridge> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_
