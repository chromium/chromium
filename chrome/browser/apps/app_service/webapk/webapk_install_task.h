// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features_parser.h"
#include "ash/components/arc/mojom/webapk.mojom.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace webapk {
class WebApk;
}

namespace web_app {
class WebAppProvider;
}

namespace apps {

// Installs or updates (as appropriate) the WebAPK for a specific app.
class WebApkInstallTask {
  using ResultCallback = base::OnceCallback<void(bool success)>;

 public:
  WebApkInstallTask(Profile* profile, const std::string& app_id);
  WebApkInstallTask(const WebApkInstallTask&) = delete;
  WebApkInstallTask& operator=(const WebApkInstallTask&) = delete;

  ~WebApkInstallTask();

  void Start(ResultCallback callback);

  const std::string& app_id() { return app_id_; }

  void SetTimeoutForTesting(base::TimeDelta timeout) {
    minter_timeout_ = timeout;
  }

 private:
  void LoadWebApkInfo(std::unique_ptr<webapk::WebApk> webapk);
  void OnWebApkInfoLoaded(std::unique_ptr<webapk::WebApk> webapk,
                          arc::mojom::WebApkInfoPtr result);
  void OnArcFeaturesLoaded(std::unique_ptr<webapk::WebApk> webapk,
                           std::optional<arc::ArcFeatures> arc_features);
  void OnLoadedIcon(std::unique_ptr<webapk::WebApk> webapk,
                    web_app::IconPurpose purpose,
                    std::vector<uint8_t> data);
  void OnProtoSerialized(std::optional<std::string> serialized_proto);
  void OnUrlLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnInstallComplete(const std::string& package_name,
                         arc::mojom::WebApkInstallResult result);

  void FetchWebApkInfoFromCrosapi();
  void OnWebApkInfoFetchedFromCrosapi(
      crosapi::mojom::WebApkCreationParamsPtr webapk_creation_params);

  // Delivers a result to the callback. The callback can delete this task, so no
  // further work should be done after calling this method.
  void DeliverResult(WebApkInstallStatus status);

  const raw_ptr<Profile> profile_;
  const raw_ptr<web_app::WebAppProvider> web_app_provider_;

  arc::mojom::WebApkInfoPtr web_apk_info_;
  const std::string app_id_;

  // If we are updating an existing WebAPK, contains the package name of the
  // existing WebAPK. Empty if this is an installation for a new WebAPK.
  std::optional<std::string> package_name_to_update_;

  // Timeout for a response to arrive from the WebAPK minter.
  base::TimeDelta minter_timeout_;

  ResultCallback result_callback_;

  // Loader used to request a WebAPK from the minter.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Fails the installation if the request to the WebAPK minter takes too long.
  base::OneShotTimer timer_;

  // TODO(crbug.com/40199484): Consider passing app short name to
  // OnProtoSerialized() and OnUrlLoaderComplete().
  std::string app_short_name_;

  base::WeakPtrFactory<WebApkInstallTask> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_
