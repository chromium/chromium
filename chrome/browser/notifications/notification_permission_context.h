// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_context_base.h"
#include "extensions/buildflags/buildflags.h"

class GURL;

// This permission context is responsible for getting, deciding on and updating
// the notification permission for a particular website or extension. This
// permission guards use of the Notification API, the Web Push API and the
// chrome.notifications extension APIs.
//
// https://notifications.spec.whatwg.org/
// https://w3c.github.io/push-api/
// https://developer.chrome.com/apps/notifications
//
// -----------------------------------------------------------------------------
// Websites
// -----------------------------------------------------------------------------
//
// Websites running top-level on a secure origin are able to request the
// notification permission by calling Notification.requestPermission(), or
// implicitly through PushManager.subscribe(). Requests from insecure origins
// and sub-frames are rejected, but can use previously granted permission.
//
// When a website running in a sub-frame checks whether permission has been
// granted, and no decision has been made yet (i.e. CONTENT_SETTING_ASK),
// CONTENT_SETTING_BLOCK will be returned to reflect the fact that permission
// cannot be requested.
//
// When the user rejects a notification permission request, the WebContents will
// be prevented from requesting the permission again (regardless of origin)
// until a user-initiated navigation occurs. This stops users from being locked
// in to cross-origin request loops that may be hard to escape from.
//
// ANDROID O+
//
//     On Android O and beyond, notification channels will be used for storing
//     website permissions as opposed to regular preferences. The permissions
//     are configurable by the user in system UI, and will be backed up and
//     restored by the operating system at the appropriate times.
//
//         Settings > Apps & notifications > Chrome > Notifications > Sites
//
//     The NotificationChannelsProviderAndroid implements this behaviour, and
//     is added as a content setting provider to the HostContentSettingsMap.
//
// https://developer.android.com/guide/topics/ui/notifiers/notifications#ManageChannels
//
// INCOGNITO
//
//    The notification permission is not available in Incognito browsing mode
//    because the expected behaviour regarding short-lived push subscriptions
//    has not been decided upon. Use of native notification centers, where
//    Chrome hands over potentially sensitive information to the underlying
//    operating system, will also be a consideration in this decision.
//
//    An explicit design goal of Incognito mode is that developers should not be
//    able to recognise that it is being used. Applicable permission requests
//    are therefore automatically rejected after a random number of seconds in
//    the range [1.0, 2.0).
//
// -----------------------------------------------------------------------------
// Extensions
// -----------------------------------------------------------------------------
//
// Extensions that wish to use notifications should declare the "notifications"
// permission in their manifest. The user will be prompted to accept this when
// the extension is being installed, or installation will be aborted.
//
// An installed extension that declared the "notifications" permission in their
// manifest is assumed to have full permission. This can be overridden by the
// NotifierStateTracker, configurable through UI affordances such as the Chrome
// OS Notification Center and the right click -> "disable notifications" option.
//
// Extensions that do not declare the "notifications" permission in their
// manifest will be treated as regular websites.
class NotificationPermissionContext
    : public permissions::PermissionContextBase {
 public:
  // Helper method for updating the permission state of |origin| to |setting|.
  static void UpdatePermission(content::BrowserContext* browser_context,
                               const GURL& origin,
                               ContentSetting setting);

  explicit NotificationPermissionContext(
      content::BrowserContext* browser_context);
  ~NotificationPermissionContext() override;

  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationPermissionContextTest,
                           WebNotificationsTopLevelOriginOnly);
  friend class NotificationPermissionContextTest;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns the notification permission status for |origin| if it describes an
  // extension. CONTENT_SETTING_ASK will be returned when it's not an extension
  // that has the "notifications" permission declared in their manifest.
  ContentSetting GetPermissionStatusForExtension(const GURL& origin) const;
#endif

  // PermissionContextBase implementation.
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;

  base::WeakPtrFactory<NotificationPermissionContext> weak_factory_ui_thread_{
      this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_
