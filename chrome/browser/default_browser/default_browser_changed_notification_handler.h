// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CHANGED_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CHANGED_NOTIFICATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/notifications/notification_handler.h"

class Profile;

namespace default_browser {

class DefaultBrowserController;

// Notification handler for showing default browser changed notification on
// supported platforms. This is instantiated and owned by the
// NotificationDisplayService. It is bound with the the notification handle type
// `NotificationHandler::Type::DEFAULT_BROWSER_CHANGED`.
class DefaultBrowserChangedNotificationHandler : public NotificationHandler {
 public:
  DefaultBrowserChangedNotificationHandler();

  DefaultBrowserChangedNotificationHandler(
      const DefaultBrowserChangedNotificationHandler&) = delete;
  DefaultBrowserChangedNotificationHandler& operator=(
      const DefaultBrowserChangedNotificationHandler&) = delete;

  ~DefaultBrowserChangedNotificationHandler() override;

 private:
  void OnShow(Profile* profile, const std::string& notification_id) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;

  // It manages the state and metrics for the active notification.
  // It is created when the notification is shown and destroyed when closed.
  std::unique_ptr<DefaultBrowserController> controller_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_CHANGED_NOTIFICATION_HANDLER_H_
