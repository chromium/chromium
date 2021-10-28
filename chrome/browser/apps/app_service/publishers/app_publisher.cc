// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/app_publisher.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

namespace apps {

AppPublisher::AppPublisher(Profile* profile)
    : proxy_(AppServiceProxyFactory::GetForProfile(profile)) {}

AppPublisher::~AppPublisher() = default;

// static
std::unique_ptr<App> AppPublisher::MakeApp(AppType app_type,
                                           const std::string& app_id,
                                           Readiness readiness,
                                           const std::string& name) {
  std::unique_ptr<App> app = std::make_unique<App>(app_type, app_id);

  app->app_type = app_type;
  app->app_id = app_id;
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;
  return app;
}

void AppPublisher::Publish(std::unique_ptr<App> app) {
// TODO(crbug.com/1253250): Support Lacros. Remove this Lacros macro.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return;
#else
  std::vector<std::unique_ptr<App>> apps;
  apps.push_back(std::move(app));
  proxy_->OnApps(std::move(apps), apps::AppType::kUnknown,
                 false /* should_notify_initialized */);
#endif
}

void AppPublisher::Publish(std::vector<std::unique_ptr<App>> apps) {
// TODO(crbug.com/1253250): Support Lacros. Remove this Lacros macro.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return;
#else
  proxy_->OnApps(std::move(apps), AppType::kUnknown,
                 false /* should_notify_initialized */);
#endif
}

}  // namespace apps
