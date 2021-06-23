// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/web_applications/app_service/web_apps.h"
#include "components/services/app_service/app_service_impl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/fake_lacros_web_apps_host.h"
#include "chrome/browser/web_applications/app_service/web_apps_publisher_host.h"
#endif

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/1144877): Remove after the actual lacros web app host code
// completed.
const bool kUseFakeWebAppsHost = false;
#endif

AppServiceProxy::AppServiceProxy(Profile* profile)
    : AppServiceProxyBase(profile) {
  Initialize();
}

AppServiceProxy::~AppServiceProxy() = default;

void AppServiceProxy::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  AppServiceProxyBase::Initialize();

  if (!app_service_.is_connected()) {
    return;
  }

  web_apps_ = std::make_unique<web_app::WebApps>(app_service_, profile_);
  extension_apps_ = std::make_unique<ExtensionApps>(app_service_, profile_);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (kUseFakeWebAppsHost) {
    // Create a fake lacros web app host in the lacros-chrome for testing lacros
    // web app publishing. This will be removed after the actual lacros web app
    // host code is created.
    fake_lacros_web_apps_host_ = std::make_unique<FakeLacrosWebAppsHost>();
    fake_lacros_web_apps_host_->Init();
  } else {
    web_apps_publisher_host_ =
        std::make_unique<web_app::WebAppsPublisherHost>(profile_);
    web_apps_publisher_host_->Init();
  }
#endif

  // Asynchronously add app icon source, so we don't do too much work in the
  // constructor.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppServiceProxy::AddAppIconSource,
                                weak_ptr_factory_.GetWeakPtr(), profile_));
}

void AppServiceProxy::Uninstall(const std::string& app_id,
                                apps::mojom::UninstallSource uninstall_source,
                                gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kWeb) {
    web_app::WebApps::UninstallImpl(profile_, app_id, uninstall_source,
                                    parent_window);
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  app_service_impl_->FlushMojoCallsForTesting();
  receivers_.FlushForTesting();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
web_app::WebAppsPublisherHost*
AppServiceProxy::WebAppsPublisherHostForTesting() {
  return web_apps_publisher_host_.get();
}
#endif

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

void AppServiceProxy::Shutdown() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (web_apps_publisher_host_) {
    web_apps_publisher_host_->Shutdown();
  }
#endif
}

}  // namespace apps
