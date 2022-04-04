// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/app_publisher.h"

#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"

namespace apps {

AppPublisher::AppPublisher(AppServiceProxy* proxy) : proxy_(proxy) {
  DCHECK(proxy);
}

AppPublisher::~AppPublisher() = default;

// static
AppPtr AppPublisher::MakeApp(AppType app_type,
                             const std::string& app_id,
                             Readiness readiness,
                             const std::string& name,
                             InstallReason install_reason,
                             InstallSource install_source) {
  auto app = std::make_unique<App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;

  app->install_reason = install_reason;
  app->install_source = install_source;

  app->is_platform_app = false;
  app->recommendable = true;
  app->searchable = true;
  app->paused = false;

  return app;
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
void AppPublisher::RegisterPublisher(AppType app_type) {
  proxy_->RegisterPublisher(app_type, this);
}

void AppPublisher::Publish(AppPtr app) {
  if (!proxy_) {
    NOTREACHED();
    return;
  }

  std::vector<AppPtr> apps;
  apps.push_back(std::move(app));
  proxy_->OnApps(std::move(apps), apps::AppType::kUnknown,
                 false /* should_notify_initialized */);
}

void AppPublisher::Publish(std::vector<AppPtr> apps,
                           AppType app_type,
                           bool should_notify_initialized) {
  if (!proxy_) {
    NOTREACHED();
    return;
  }
  proxy_->OnApps(std::move(apps), app_type, should_notify_initialized);
}
#endif

}  // namespace apps
