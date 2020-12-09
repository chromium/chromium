// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_notifications.h"

#include "base/containers/contains.h"

namespace apps {

AppNotifications::AppNotifications() = default;

AppNotifications::~AppNotifications() = default;

void AppNotifications::AddNotification(const std::string& app_id,
                                       const std::string& notification_id) {
  app_id_to_notification_ids_[app_id].insert(notification_id);
  notification_id_to_app_ids_[notification_id].insert(app_id);
}

void AppNotifications::RemoveNotification(const std::string& notification_id) {
  auto it = notification_id_to_app_ids_.find(notification_id);
  DCHECK(it != notification_id_to_app_ids_.end());

  for (const auto& app_id : it->second) {
    auto app_id_it = app_id_to_notification_ids_.find(app_id);
    app_id_it->second.erase(notification_id);
    if (app_id_it->second.empty()) {
      app_id_to_notification_ids_.erase(app_id_it);
    }
  }
  notification_id_to_app_ids_.erase(it);
}

void AppNotifications::RemoveNotificationsForApp(const std::string& app_id) {
  auto it = app_id_to_notification_ids_.find(app_id);
  if (it == app_id_to_notification_ids_.end()) {
    return;
  }

  for (const auto& notification_id : it->second) {
    auto notification_id_it = notification_id_to_app_ids_.find(notification_id);
    notification_id_it->second.erase(app_id);
    if (notification_id_it->second.empty()) {
      notification_id_to_app_ids_.erase(notification_id_it);
    }
  }
  app_id_to_notification_ids_.erase(it);
}

bool AppNotifications::HasNotification(const std::string& app_id) {
  return base::Contains(app_id_to_notification_ids_, app_id);
}

std::set<std::string> AppNotifications::GetAppIdsForNotification(
    const std::string& notification_id) {
  auto it = notification_id_to_app_ids_.find(notification_id);
  if (it == notification_id_to_app_ids_.end()) {
    return {};
  }
  return it->second;
}

apps::mojom::AppPtr AppNotifications::GetAppWithHasBadgeStatus(
    apps::mojom::AppType app_type,
    const std::string& app_id) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->has_badge = (HasNotification(app_id))
                       ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  return app;
}

}  // namespace apps
