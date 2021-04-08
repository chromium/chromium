// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_
#define CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/notification_types.h"
#else
#include "content/public/browser/notification_types.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#define PREVIOUS_END extensions::NOTIFICATION_EXTENSIONS_END
#else
#define PREVIOUS_END content::NOTIFICATION_CONTENT_END
#endif

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
  NOTIFICATION_CHROME_START = PREVIOUS_END,

  // Application-wide ----------------------------------------------------------

  // This message is sent when the application is terminating (the last
  // browser window has shutdown as part of an explicit user-initiated exit,
  // or the user closed the last browser window on Windows/Linux and there are
  // no BackgroundContents keeping the browser running). No source or details
  // are passed.
  // TODO(https://crbug.com/1174781): Remove.
  NOTIFICATION_APP_TERMINATING = NOTIFICATION_CHROME_START,

#if defined(OS_MAC)
  // This notification is sent when the app has no key window, such as when
  // all windows are closed but the app is still active. No source or details
  // are provided.
  // TODO(https://crbug.com/1174783): Remove.
  NOTIFICATION_NO_KEY_WINDOW,
#endif

  // Authentication ----------------------------------------------------------

  // This is sent when a login prompt is shown.  The source is the
  // Source<NavigationController> for the tab in which the prompt is shown.
  // Details are a LoginNotificationDetails which provide the LoginHandler
  // that should be given authentication.
  // TODO(https://crbug.com/1174785): Remove.
  NOTIFICATION_AUTH_NEEDED,

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

  // Profiles -----------------------------------------------------------------

  // Use ProfileManagerObserver::OnProfileAdded instead of this notification.
  // Sent after a Profile has been added to ProfileManager.
  // The details are none and the source is the new profile.
  // TODO(https://crbug.com/1174720): Remove. See also
  // https://crbug.com/1038437.
  NOTIFICATION_PROFILE_ADDED,

  // Printing ----------------------------------------------------------------

  // Notification from PrintJob that an event occurred. It can be that a page
  // finished printing or that the print job failed. Details is
  // PrintJob::EventDetails. Source is a PrintJob.
  // TODO(https://crbug.com/796051): Remove.
  NOTIFICATION_PRINT_JOB_EVENT,

  // Sent when a PrintJob has been released.
  // Source is the WebContents that holds the print job.
  // TODO(https://crbug.com/1174788): Remove.
  NOTIFICATION_PRINT_JOB_RELEASED,

  // Misc --------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sent when a network error message is displayed on the WebUI login screen.
  // First paint event of this fires NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE.
  // TODO(https://crbug.com/1174791): Remove.
  NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,

  // Sent when the specific part of login/lock WebUI is considered to be
  // visible. That moment is tracked as the first paint event after one of the:
  // NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN
  //
  // Possible series of notifications:
  // 1. Boot into fresh OOBE
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 2. Boot into user pods list (normal boot). Same for lock screen.
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 3. Boot into GAIA sign in UI (user pods display disabled or no users):
  //    if no network is connected or flaky network
  //    (NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN +
  //     NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE)
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // 4. Boot into retail mode
  //    NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE
  // TODO(https://crbug.com/1174793): Remove.
  NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,

  // Sent when the screen lock state has changed. The source is
  // ScreenLocker and the details is a bool specifing that the
  // screen is locked. When details is a false, the source object
  // is being deleted, so the receiver shouldn't use the screen locker
  // object.
  // TODO(https://crbug.com/1174796): Remove.
  NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
#endif

#if defined(TOOLKIT_VIEWS)
  // Notification that the nested loop using during tab dragging has returned.
  // Used for testing.
  // TODO(https://crbug.com/1174797): Remove.
  NOTIFICATION_TAB_DRAG_LOOP_DONE,
#endif

  // Sent when the applications in the NTP app launcher have been reordered.
  // The details, if not NoDetails, is the std::string ID of the extension that
  // was moved.
  // TODO(https://crbug.com/1174798): Remove.
  NOTIFICATION_APP_LAUNCHER_REORDERED,

  // Sent when an app is installed and an NTP has been shown. Source is the
  // WebContents that was shown, and Details is the string ID of the extension
  // which was installed.
  // TODO(https://crbug.com/1174799): Remove.
  NOTIFICATION_APP_INSTALLED_TO_NTP,

  // Note:-
  // Currently only Content and Chrome define and use notifications.
  // Custom notifications not belonging to Content and Chrome should start
  // from here.
  NOTIFICATION_CHROME_END,
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

#endif  // CHROME_BROWSER_CHROME_NOTIFICATION_TYPES_H_
