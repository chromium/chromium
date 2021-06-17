// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/arc/arc_features_parser.h"
#include "components/arc/mojom/webapk.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace webapk {
class WebApk;
}

namespace web_app {
class WebAppProviderBase;
}

namespace apps {

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
                           absl::optional<arc::ArcFeatures> arc_features);
  void OnLoadedIcon(std::unique_ptr<webapk::WebApk> webapk,
                    IconPurpose purpose,
                    std::vector<uint8_t> data);
  void OnProtoSerialized(absl::optional<std::string> serialized_proto);
  void OnUrlLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnInstallComplete(const std::string& package_name,
                         arc::mojom::WebApkInstallResult result);

  // Delivers a result to the callback. The callback can delete this task, so no
  // further work should be done after calling this method.
  void DeliverResult(bool success);

  Profile* const profile_;
  web_app::WebAppProviderBase* web_app_provider_;

  arc::mojom::WebApkInfoPtr web_apk_info_;
  const std::string app_id_;

  // Timeout for a response to arrive from the WebAPK minter.
  base::TimeDelta minter_timeout_;

  ResultCallback result_callback_;

  // Loader used to request a WebAPK from the minter.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Fails the installation if the request to the WebAPK minter takes too long.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<WebApkInstallTask> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_
