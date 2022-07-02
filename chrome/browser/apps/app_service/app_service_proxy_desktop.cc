// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

AppServiceProxy::AppServiceProxy(Profile* profile)
    : AppServiceProxyBase(profile) {}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  AppServiceProxyBase::Initialize();

  if (!app_service_.is_connected()) {
    return;
  }

  publisher_host_ = std::make_unique<PublisherHost>(this);
}

void AppServiceProxy::Uninstall(const std::string& app_id,
                                apps::mojom::UninstallSource uninstall_source,
                                gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::AppType::kWeb) {
    web_app::UninstallImpl(web_app::WebAppProvider::GetForWebApps(profile_),
                           app_id, uninstall_source, parent_window);
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  app_service_mojom_impl_->FlushMojoCallsForTesting();
  receivers_.FlushForTesting();
  web_app::WebAppProvider::GetForTest(profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();
}

void AppServiceProxy::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  if (app_service_.is_connected()) {
    app_service_->SetRunOnOsLoginMode(
        ConvertAppTypeToMojomAppType(app_registry_cache_.GetAppType(app_id)),
        app_id, run_on_os_login_mode);
  }
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

}  // namespace apps
