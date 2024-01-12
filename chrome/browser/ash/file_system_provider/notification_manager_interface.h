// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_

#include "base/functional/callback.h"

namespace ash::file_system_provider {

// Handles notifications related to provided the file system.
class NotificationManagerInterface {
 public:
  // Result of a notification.
  enum NotificationResult { ABORT, CONTINUE };

  // Callback for handling result of a notification.
  typedef base::OnceCallback<void(NotificationResult)> NotificationCallback;

  NotificationManagerInterface() = default;

  NotificationManagerInterface(const NotificationManagerInterface&) = delete;
  NotificationManagerInterface& operator=(const NotificationManagerInterface&) =
      delete;

  virtual ~NotificationManagerInterface() = default;

  // Shows a notification about the request being unresponsive. The |callback|
  // is called when the notification is closed.
  virtual void ShowUnresponsiveNotification(int id,
                                            NotificationCallback callback) = 0;

  // Hides a notification previously shown with |id|.
  virtual void HideUnresponsiveNotification(int id) = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_
