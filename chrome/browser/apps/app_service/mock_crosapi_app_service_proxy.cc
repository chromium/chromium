// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/mock_crosapi_app_service_proxy.h"

#include "base/notreached.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"

namespace apps {

MockCrosapiAppServiceProxy::MockCrosapiAppServiceProxy() {
  run_loop_ = std::make_unique<base::RunLoop>();
}

MockCrosapiAppServiceProxy::~MockCrosapiAppServiceProxy() = default;

void MockCrosapiAppServiceProxy::Wait() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void MockCrosapiAppServiceProxy::RegisterAppServiceSubscriber(
    mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber) {
  NOTIMPLEMENTED();
}
void MockCrosapiAppServiceProxy::Launch(
    crosapi::mojom::LaunchParamsPtr launch_params) {
  launched_apps_.push_back(std::move(launch_params));
  run_loop_->Quit();
}

void MockCrosapiAppServiceProxy::LaunchWithResult(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchWithResultCallback callback) {
  launched_apps_.push_back(std::move(launch_params));
  std::move(callback).Run(ConvertLaunchResultToMojomLaunchResult(
      LaunchResult(LaunchResult::State::SUCCESS)));
  run_loop_->Quit();
}

void MockCrosapiAppServiceProxy::LoadIcon(const std::string& app_id,
                                          IconKeyPtr icon_key,
                                          IconType icon_type,
                                          int32_t size_hint_in_dip,
                                          apps::LoadIconCallback callback) {
  // TODO(crbug.com/1309024): Implement this.
  NOTIMPLEMENTED();
}
void MockCrosapiAppServiceProxy::AddPreferredApp(
    const std::string& app_id,
    crosapi::mojom::IntentPtr intent) {
  // TODO(crbug.com/1309024): Implement this.
  NOTIMPLEMENTED();
}
void MockCrosapiAppServiceProxy::ShowAppManagementPage(
    const std::string& app_id) {
  // TODO(crbug.com/1309024): Implement this.
  NOTIMPLEMENTED();
}

void MockCrosapiAppServiceProxy::SetSupportedLinksPreference(
    const std::string& app_id) {
  supported_link_apps_.push_back(std::move(app_id));
  run_loop_->Quit();
}

void MockCrosapiAppServiceProxy::UninstallSilently(
    const std::string& app_id,
    UninstallSource uninstall_source) {}

}  // namespace apps
