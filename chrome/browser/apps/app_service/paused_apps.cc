// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/paused_apps.h"

#include "base/containers/contains.h"

namespace apps {

PausedApps::PausedApps() = default;

PausedApps::~PausedApps() = default;

// static
apps::mojom::AppPtr PausedApps::GetAppWithPauseStatus(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    bool paused) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->paused = (paused) ? apps::mojom::OptionalBool::kTrue
                         : apps::mojom::OptionalBool::kFalse;
  return app;
}

bool PausedApps::MaybeAddApp(const std::string& app_id) {
  auto ret = paused_apps_.insert(app_id);
  return ret.second;
}

bool PausedApps::MaybeRemoveApp(const std::string& app_id) {
  return paused_apps_.erase(app_id) != 0;
}

bool PausedApps::IsPaused(const std::string& app_id) {
  return base::Contains(paused_apps_, app_id);
}

}  // namespace apps
