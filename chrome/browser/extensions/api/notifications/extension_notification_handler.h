// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "extensions/browser/event_router.h"

class Profile;

namespace extensions {

// Handler for notifications shown by extensions. Will be created and owned by
// the NativeNotificationDisplayService.
class ExtensionNotificationHandler : public NotificationHandler {
 public:
  ExtensionNotificationHandler();
  ~ExtensionNotificationHandler() override;

  // Extracts an extension ID from the URL for an app window, or an empty string
  // if the URL is not a valid app window URL.
  static std::string GetExtensionId(const GURL& url);

  // NotificationHandler implementation.
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
  void DisableNotifications(Profile* profile, const GURL& origin) override;

 protected:
  // Overriden in unit tests.
  virtual void SendEvent(Profile* profile,
                         const std::string& extension_id,
                         events::HistogramValue histogram_value,
                         const std::string& name,
                         EventRouter::UserGestureState user_gesture,
                         std::unique_ptr<base::ListValue> args);

  DISALLOW_COPY_AND_ASSIGN(ExtensionNotificationHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_
