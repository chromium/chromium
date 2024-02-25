// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MOCK_CROSAPI_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MOCK_CROSAPI_APP_SERVICE_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace apps {
class MockCrosapiAppServiceProxy : public crosapi::mojom::AppServiceProxy {
 public:
  MockCrosapiAppServiceProxy();
  ~MockCrosapiAppServiceProxy() override;
  void Wait();
  const std::vector<crosapi::mojom::LaunchParamsPtr>& launched_apps() const {
    return launched_apps_;
  }
  const std::vector<std::string>& supported_link_apps() const {
    return supported_link_apps_;
  }

 private:
  // crosapi::mojom::AppServiceProxy:
  void RegisterAppServiceSubscriber(
      mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber)
      override;
  void Launch(crosapi::mojom::LaunchParamsPtr launch_params) override;
  void LaunchWithResult(crosapi::mojom::LaunchParamsPtr launch_params,
                        LaunchWithResultCallback callback) override;
  void LoadIcon(const std::string& app_id,
                IconKeyPtr icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                apps::LoadIconCallback callback) override;
  void AddPreferredAppDeprecated(const std::string& app_id,
                                 crosapi::mojom::IntentPtr intent) override {}
  void ShowAppManagementPage(const std::string& app_id) override;
  void SetSupportedLinksPreference(const std::string& app_id) override;
  void UninstallSilently(const std::string& app_id,
                         UninstallSource uninstall_source) override;
  void InstallApp(crosapi::mojom::InstallAppParamsPtr params,
                  InstallAppCallback callback) override;

  std::vector<crosapi::mojom::LaunchParamsPtr> launched_apps_;
  std::vector<std::string> supported_link_apps_;
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MOCK_CROSAPI_APP_SERVICE_PROXY_H_
