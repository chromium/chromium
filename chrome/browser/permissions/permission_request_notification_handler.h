// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_HANDLER_H_

#include <map>
#include <string>

#include "chrome/browser/notifications/notification_handler.h"

// Handles PERMISSION_REQUEST nofication actions, by passing them to the
// appropriate delegate. The delegates need to register/deregister with this
// object on their own. This class is needed because notifications are able to
// outlive the browser process, so message_center::NotificationDelegate serves
// no function anymore. Instead, a NotificationHandler subclass is created for
// each notification type and registered in the NotificationDisplayServiceImpl
// constructor.
class PermissionRequestNotificationHandler : public NotificationHandler {
 public:
  class Delegate {
   public:
    // Called when the notification is closed.
    virtual void Close() = 0;
    // Called when a notification button is clicked.
    virtual void Click(int button_index) = 0;
  };

  PermissionRequestNotificationHandler();
  ~PermissionRequestNotificationHandler() override;

  // Register a delegate for a particular notification. The delegate needs to
  // manage itself by calling RemoveNotificationDelegate to ensure no dangling
  // pointers are left in the map.
  void AddNotificationDelegate(const std::string& notification_id,
                               Delegate* notification_delegate);
  // De-register the delegate for a particular notification.
  void RemoveNotificationDelegate(const std::string& notification_id);

  // NotificationHandler:
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override;

  // Functions for testing only.
  const std::map<std::string, Delegate*>& notification_delegates_for_testing() {
    return notification_delegates_;
  }

 protected:
  // Retrieves the delegate for a particular |notification_id|, or nullptr if
  // there is none.
  Delegate* GetNotificationDelegate(const std::string& notification_id);

  // Map of notifications (by id) to delegates.
  std::map<std::string, Delegate*> notification_delegates_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestNotificationHandler);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_HANDLER_H_
