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
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/arc/arc_features_parser.h"
#include "components/arc/mojom/webapk.mojom-forward.h"
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

 private:
  void OnArcFeaturesLoaded(std::unique_ptr<webapk::WebApk> webapk,
                           ResultCallback callback,
                           absl::optional<arc::ArcFeatures> arc_features);
  void OnLoadedIcon(std::unique_ptr<webapk::WebApk> webapk,
                    ResultCallback callback,
                    IconPurpose purpose,
                    std::vector<uint8_t> data);
  void OnProtoSerialized(ResultCallback callback, std::string serialized_proto);
  void OnUrlLoaderComplete(ResultCallback callback,
                           std::unique_ptr<std::string> response_body);
  void OnInstallComplete(const std::string& package_name,
                         ResultCallback callback,
                         arc::mojom::WebApkInstallResult result);

  Profile* const profile_;
  web_app::WebAppProviderBase* web_app_provider_;

  const std::string app_id_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<WebApkInstallTask> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_TASK_H_
