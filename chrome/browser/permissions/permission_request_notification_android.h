// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_ANDROID_H_

#include <memory>
#include <string>

#include "chrome/browser/permissions/permission_request_notification_handler.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace message_center {
class Notification;
}  // namespace message_center

class NotificationDisplayServiceImpl;
class Profile;

// Class for displaying a permission prompt as a notification. Uses
// the notification display service to show the notification.
class PermissionRequestNotificationAndroid final
    : public PermissionRequestNotificationHandler::Delegate {
 public:
  ~PermissionRequestNotificationAndroid();

  // The |delegate| needs to outlive the current object.
  static std::unique_ptr<PermissionRequestNotificationAndroid> Create(
      content::WebContents* web_contents,
      PermissionPrompt::Delegate* delegate);

  // Returns true if we should show the permission request as a notification.
  static bool ShouldShowAsNotification(Profile* profile,
                                       ContentSettingsType type);

  // Converts an origin string into a notification id.
  static std::string NotificationIdForOrigin(const std::string& origin);

  // The behavior that this notification should have after tab switching.
  static PermissionPrompt::TabSwitchingBehavior GetTabSwitchingBehavior();

 private:
  // PermissionRequestNotificationHandler::Delegate
  void Close() override;
  void Click(int button_index) override;

  PermissionRequestNotificationAndroid(content::WebContents* web_contents,
                                       PermissionPrompt::Delegate* delegate);

  // Remove the notification.
  void DismissNotification();

  // The |delegate_| is used for sending use actions to the request
  // manager. It needs to outlive the current object.
  PermissionPrompt::Delegate* delegate_;

  // The notification that it managed by this object.
  std::unique_ptr<message_center::Notification> notification_;

  // NotificationDisplayServiceImpl to be used for sending notifications. This
  // is profile-bound and as such outlives this object so holding a raw pointer
  // is fine.
  NotificationDisplayServiceImpl* notification_display_service_;

  // PermissionRequestNotificationHandler that redirects events to this
  // delegate. This outlives this object as it it managed by the above
  // profile-bound |notification_display_service_| and as such holding a raw
  // pointer is fine.
  PermissionRequestNotificationHandler*
      permission_request_notification_handler_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestNotificationAndroid);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_NOTIFICATION_ANDROID_H_
