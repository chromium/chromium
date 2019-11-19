// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

class Profile;

namespace message_center {
class Notification;
}

// Helper class that enables use of the NotificationDisplayService in tests. The
// Profile* passed when constructing an instance may outlive this class.
//
// This class must only be used for testing purposes. Unlike most production
// NotificationDisplayService implementations, all operations on this tester are
// synchronous.
class NotificationDisplayServiceTester {
 public:
  // |profile| - the profile with which |display_service_| will be associated.
  //     It can be nullptr, in which case |display_service_| will be set up as a
  //     system notification display service.
  explicit NotificationDisplayServiceTester(Profile* profile);
  ~NotificationDisplayServiceTester();

  // Returns the currently active tester, if any.
  static NotificationDisplayServiceTester* Get();

  // Sets |closure| to be invoked when any notification has been added.
  void SetNotificationAddedClosure(base::RepeatingClosure closure);

  // Sets |closure| to be invoked when any notification has been closed.
  void SetNotificationClosedClosure(base::RepeatingClosure closure);

  // Synchronously gets a vector of the displayed Notifications for the |type|.
  std::vector<message_center::Notification> GetDisplayedNotificationsForType(
      NotificationHandler::Type type);

  const NotificationCommon::Metadata* GetMetadataForNotification(
      const message_center::Notification& notification);

  base::Optional<message_center::Notification> GetNotification(
      const std::string& notification_id) const;

  // Simulates the notification identified by |notification_id| being clicked
  // on, optionally with the given |action_index| and |reply|.
  void SimulateClick(NotificationHandler::Type notification_type,
                     const std::string& notification_id,
                     base::Optional<int> action_index,
                     base::Optional<base::string16> reply);

  // Simulates a click on the settings button of the notification identified by
  // |notification_id|.
  void SimulateSettingsClick(NotificationHandler::Type notification_type,
                             const std::string& notification_id);

  // Simulates the notification identified by |notification_id| being closed due
  // to external events, such as the user dismissing it when |by_user| is set.
  // When |silent| is set, the notification handlers won't be informed of the
  // change to immitate behaviour of operating systems that don't inform apps
  // about removed notifications.
  void RemoveNotification(NotificationHandler::Type type,
                          const std::string& notification_id,
                          bool by_user,
                          bool silent = false);

  // Removes all notifications of the given |type|.
  void RemoveAllNotifications(NotificationHandler::Type type, bool by_user);

  // Sets a |delegate| to notify when ProcessNotificationOperation is called.
  void SetProcessNotificationOperationDelegate(
      const StubNotificationDisplayService::
          ProcessNotificationOperationCallback& delegate);

 private:
  void OnProfileShutdown();

  Profile* profile_;
  StubNotificationDisplayService* display_service_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      profile_shutdown_subscription_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayServiceTester);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_
