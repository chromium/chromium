// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class NotificationDisplayService;
class Profile;

namespace message_center {
class Notification;
}

namespace extensions {

// Helper class for displaying notifications through the notification display
// service. The NDS supports notifications that can outlive the browser process,
// and therefore does not retain as much information as is necessary to support
// the extensions API. (Notably the ability to partly update notifications.)
class ExtensionNotificationDisplayHelper : public KeyedService {
 public:
  explicit ExtensionNotificationDisplayHelper(Profile* profile);
  ~ExtensionNotificationDisplayHelper() override;

  // Displays the |notification| using the notification display service.
  void Display(const message_center::Notification& notification);

  // Returns the notification identified by |notification_id| if it is currently
  // visible. May return a nullptr.
  message_center::Notification* GetByNotificationId(
      const std::string& notification_id);

  // Returns a set with the IDs of all notifications that are currently being
  // shown on behalf of the |extension_origin|.
  std::set<std::string> GetNotificationIdsForExtension(
      const GURL& extension_origin) const;

  // Removes stored state for the notification identified by |notification_id|.
  // Returns whether there was local state.
  bool EraseDataForNotificationId(const std::string& notification_id);

  // Closes the notification identified by |notification_id|. Returns whether a
  // notification was closed in response to the call.
  bool Close(const std::string& notification_id);

  // KeyedService overrides:
  void Shutdown() override;

 private:
  using NotificationVector =
      std::vector<std::unique_ptr<message_center::Notification>>;

  // Returns the notification display service instance to communicate with.
  NotificationDisplayService* GetDisplayService();

  // The Profile instance that owns this keyed service.
  Profile* profile_;

  // Vector of notifications that are being shown for extensions.
  NotificationVector notifications_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNotificationDisplayHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_H_
