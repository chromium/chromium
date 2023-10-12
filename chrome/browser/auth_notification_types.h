// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTH_NOTIFICATION_TYPES_H_
#define CHROME_BROWSER_AUTH_NOTIFICATION_TYPES_H_

#include "content/public/browser/notification_types.h"

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace chrome {

enum NotificationType {
  // This is sent when a login prompt is shown.  The source is the
  // Source<NavigationController> for the tab in which the prompt is shown.
  // Details are a LoginNotificationDetails which provide the LoginHandler
  // that should be given authentication.
  // TODO(https://crbug.com/1174785): Remove.
  NOTIFICATION_AUTH_NEEDED = content::NOTIFICATION_CONTENT_END,

  // This is sent when authentication credentials have been supplied (either
  // by the user or by an automation service), but before we've actually
  // received another response from the server.  The source is the
  // Source<NavigationController> for the tab in which the prompt was shown.
  // Details are an AuthSuppliedLoginNotificationDetails which provide the
  // LoginHandler that should be given authentication as well as the supplied
  // username and password.
  // TODO(https://crbug.com/1174785): Remove.
  NOTIFICATION_AUTH_SUPPLIED,

  // This is sent when an authentication request has been dismissed without
  // supplying credentials (either by the user or by an automation service).
  // The source is the Source<NavigationController> for the tab in which the
  // prompt was shown. Details are a LoginNotificationDetails which provide
  // the LoginHandler that should be cancelled.
  // TODO(https://crbug.com/1174785): Remove.
  NOTIFICATION_AUTH_CANCELLED,
};

}  // namespace chrome

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

#endif  // CHROME_BROWSER_AUTH_NOTIFICATION_TYPES_H_
