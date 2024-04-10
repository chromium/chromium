// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_web_apps_utils.h"

#include <memory>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

void MaybeSimulatePromiseAppInstallationEvents(apps::AppServiceProxy* proxy,
                                               apps::App* app) {
  // Check if the web app has the necessary metadata and is newly installed.
  if (!app->publisher_id.has_value()) {
    return;
  }
  bool app_exists = false;
  proxy->AppRegistryCache().ForOneApp(
      app->app_id, [&app_exists](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::Readiness::kUninstalledByUser ||
            update.Readiness() == apps::Readiness::kUninstalledByNonUser ||
            update.Readiness() == apps::Readiness::kRemoved) {
          // It's possible for a user to install, uninstall then reinstall the
          // same app. We still want to consider a reinstall as a new
          // installation and simulate its installation events.
          return;
        }
        app_exists = true;
        return;
      });
  if (app_exists) {
    return;
  }

  // Simulate the promise app installation stages.
  apps::PackageId package_id(apps::PackageType::kWeb,
                             app->publisher_id.value());

  // Register a promise app.
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->status = apps::PromiseStatus::kPending;
  promise_app->should_show = true;
  proxy->OnPromiseApp(std::move(promise_app));

  // Update the promise app status.
  apps::PromiseAppPtr promise_app_installing =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app_installing->progress = 0;
  promise_app_installing->status = apps::PromiseStatus::kInstalling;
  proxy->OnPromiseApp(std::move(promise_app_installing));
}

}  // namespace apps
