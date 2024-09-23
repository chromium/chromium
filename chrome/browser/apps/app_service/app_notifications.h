// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

// AppNotifications records the notification id for each app.
class AppNotifications {
 public:
  AppNotifications();
  ~AppNotifications();

  AppNotifications(const AppNotifications&) = delete;
  AppNotifications& operator=(const AppNotifications&) = delete;

  // Records that |app_id| has a new notification identified by
  // |notification_id|.
  void AddNotification(const std::string& app_id,
                       const std::string& notification_id);

  // Removes the notification for the given |notification_id|.
  void RemoveNotification(const std::string& notification_id);

  // Removes notifications for the given |app_id|.
  void RemoveNotificationsForApp(const std::string& app_id);

  // Returns true, if the app has notifications. Otherwise, returns false.
  bool HasNotification(const std::string& app_id);

  // Returns the set of app ids for the given |notification_id|, if
  // |notification_id| exists in |notification_id_to_app_id_|. Otherwise, return
  // an empty set.
  std::set<std::string> GetAppIdsForNotification(
      const std::string& notification_id);

  AppPtr CreateAppWithHasBadgeStatus(AppType app_type,
                                     const std::string& app_id);

 private:
  // Maps one app id to a set of all matching notification ids.
  std::map<std::string, std::set<std::string>> app_id_to_notification_ids_;

  // Maps one notification id to a set of app ids. When the notification has
  // been delivered, the MessageCenter has already deleted the notification, so
  // we can't fetch the corresponding app id when the notification is removed.
  // So we need a record of this notification, and erase it from both
  // |app_id_to_notification_id_| and |notification_id_to_app_id_|.
  std::map<std::string, std::set<std::string>> notification_id_to_app_ids_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_
