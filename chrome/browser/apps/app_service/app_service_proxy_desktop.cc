// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"

namespace apps {

AppServiceProxy::AppServiceProxy(Profile* profile)
    : AppServiceProxyBase(profile) {}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  AppServiceProxyBase::Initialize();

  if (!base::FeatureList::IsEnabled(kStopMojomAppService) &&
      !app_service_.is_connected()) {
    return;
  }

  publisher_host_ = std::make_unique<PublisherHost>(this);
}

void AppServiceProxy::Uninstall(const std::string& app_id,
                                UninstallSource uninstall_source,
                                gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::AppType::kWeb) {
    web_app::UninstallImpl(web_app::WebAppProvider::GetForWebApps(profile_),
                           app_id, uninstall_source, parent_window);
  }
}

void AppServiceProxy::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::RunOnOsLoginMode run_on_os_login_mode) {
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::AppType::kWeb) {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile_);
    provider->scheduler().SetRunOnOsLoginMode(
        app_id,
        web_app::ConvertOsLoginModeToWebAppConstants(run_on_os_login_mode),
        base::DoNothing());
  }
}

base::WeakPtr<AppServiceProxy> AppServiceProxy::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

}  // namespace apps
