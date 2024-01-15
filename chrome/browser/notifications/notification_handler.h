// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"

class GURL;
class Profile;

// Interface that enables the different kind of notifications to process
// operations coming from the user or decisions made by the underlying
// notification type.
class NotificationHandler {
 public:
  // Type of notifications that a handler can be responsible for.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.notifications
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: NotificationType
  enum class Type {
    WEB_PERSISTENT = 0,
    WEB_NON_PERSISTENT = 1,
    EXTENSION = 2,
    SEND_TAB_TO_SELF = 3,
    TRANSIENT = 4,  // A generic type for any notification that does not outlive
                    // the browser instance and is controlled by a
                    // NotificationDelegate.
    // Deprecated
    // PERMISSION_REQUEST = 5,  // A permission request that is presented to the
    //                          // user via a notification.
    SHARING = 6,
    ANNOUNCEMENT = 7,
    NEARBY_SHARE = 8,
    NOTIFICATIONS_MUTED = 9,
    TAILORED_SECURITY = 10,
    MAX = TAILORED_SECURITY,
  };

  virtual ~NotificationHandler();

  // Called after a notification has been displayed.
  virtual void OnShow(Profile* profile, const std::string& notification_id);

  // Process notification close events. The |completed_closure| must be invoked
  // on the UI thread once processing of the close event has been finished.
  virtual void OnClose(Profile* profile,
                       const GURL& origin,
                       const std::string& notification_id,
                       bool by_user,
                       base::OnceClosure completed_closure);

  // Process clicks on a notification or its buttons, depending on
  // |action_index|. The |completed_closure| must be invoked on the UI thread
  // once processing of the click event has been finished.
  virtual void OnClick(Profile* profile,
                       const GURL& origin,
                       const std::string& notification_id,
                       const std::optional<int>& action_index,
                       const std::optional<std::u16string>& reply,
                       base::OnceClosure completed_closure);

  // Called when notifications of the given origin have to be disabled.
  virtual void DisableNotifications(Profile* profile, const GURL& origin);

  // Called when the settings page for the given origin has to be opened.
  virtual void OpenSettings(Profile* profile, const GURL& origin);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
