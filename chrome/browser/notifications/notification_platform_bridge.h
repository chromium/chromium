// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"

namespace message_center {
class Notification;
}

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
  using NotificationBridgeReadyCallback =
      base::OnceCallback<void(bool /* success */)>;

  static std::unique_ptr<NotificationPlatformBridge> Create();

  // Returns whether a native bridge can handle a notification of the given
  // type. Ideally, this would always return true, but for now some platforms
  // can't handle TRANSIENT notifications.
  static bool CanHandleType(NotificationHandler::Type notification_type);

  // Returns a unique string identifier for |profile|.
  static std::string GetProfileId(Profile* profile);

  // Returns the basename for the profile corresponding to `profile_id`. This is
  // the reverse of GetProfileId().
  static base::FilePath GetProfileBaseNameFromProfileId(
      const std::string& profile_id);

  NotificationPlatformBridge(const NotificationPlatformBridge&) = delete;
  NotificationPlatformBridge& operator=(const NotificationPlatformBridge&) =
      delete;
  virtual ~NotificationPlatformBridge() {}

  // Shows a toast on screen using the data passed in |notification|.
  virtual void Display(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) = 0;

  // Closes a nofication with |notification_id| and |profile| if being
  // displayed.
  virtual void Close(Profile* profile, const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and posts
  // `callback` with the result.
  virtual void GetDisplayed(
      Profile* profile,
      GetDisplayedNotificationsCallback callback) const = 0;

  // Writes the ids of all currently displaying notifications for `origin` and
  // posts `callback` with the result.
  virtual void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const = 0;

  // Calls |callback| once |this| is initialized. The argument is
  // true if |this| is ready to be used and false if initialization
  // failed. |callback| may be called directly or from a posted task.
  virtual void SetReadyCallback(NotificationBridgeReadyCallback callback) = 0;

  // Called when display service for |profile| is being shut down (for example
  // if the profile is being destroyed). If |profile| is nullptr the system
  // notification display service is being shutdown.
  virtual void DisplayServiceShutDown(Profile* profile) = 0;

 protected:
  NotificationPlatformBridge() = default;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
