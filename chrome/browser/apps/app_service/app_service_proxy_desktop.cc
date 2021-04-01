// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"

#include "components/services/app_service/app_service_impl.h"

namespace apps {

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

  web_apps_ = std::make_unique<WebApps>(app_service_, profile_);
  extension_apps_ = std::make_unique<ExtensionApps>(app_service_, profile_);

  // Asynchronously add app icon source, so we don't do too much work in the
  // constructor.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppServiceProxy::AddAppIconSource,
                                weak_ptr_factory_.GetWeakPtr(), profile_));
}

void AppServiceProxy::Uninstall(const std::string& app_id,
                                gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kWeb) {
    WebApps::UninstallImpl(profile_, app_id, parent_window);
  }
}

void AppServiceProxy::FlushMojoCallsForTesting() {
  app_service_impl_->FlushMojoCallsForTesting();
  receivers_.FlushForTesting();
}

bool AppServiceProxy::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  return false;
}

}  // namespace apps
