// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_QUEUE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_QUEUE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/notifications/notification_blocker.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "ui/message_center/public/cpp/notification.h"

class NotificationDisplayService;

// The NotificationDisplayQueue holds on to a list of NotificationBlockers that
// determine if we should block new notifications from being displayed. During
// that time this class will hold on to new incoming notifications and display
// them once all blockers stop being active.
class NotificationDisplayQueue : public NotificationBlocker::Observer {
 public:
  using NotificationBlockers =
      std::vector<std::unique_ptr<NotificationBlocker>>;

  explicit NotificationDisplayQueue(
      NotificationDisplayService* notification_display_service);
  NotificationDisplayQueue(const NotificationDisplayQueue&) = delete;
  NotificationDisplayQueue& operator=(const NotificationDisplayQueue&) = delete;
  ~NotificationDisplayQueue() override;

  // NotificationBlocker::Observer:
  void OnBlockingStateChanged() override;

  // Returns if we should currently queue up |notification|. This is the case if
  // at least one NotificationBlocker is active and |notification_type| is a Web
  // Notification.
  bool ShouldEnqueueNotification(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification) const;

  // Enqueue the passed |notification| to be shown once no blocker is active
  // anymore. If there is already a notification with the same id queued, it
  // will be removed before adding this new one.
  void EnqueueNotification(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata);

  // Removes a queued notification by its |notification_id|. This is a no-op if
  // there was no notification queued with that id.
  void RemoveQueuedNotification(const std::string& notification_id);

  // Returns a set of the currently queued notification ids.
  std::set<std::string> GetQueuedNotificationIds() const;

  // Returns a set of the currently queued notification ids associated with
  // `origin`.
  std::set<std::string> GetQueuedNotificationIdsForOrigin(
      const GURL& origin) const;

  // Sets the list of |blockers| to be used and observes their state.
  void SetNotificationBlockers(NotificationBlockers blockers);

  // Adds |blocker| to the list of blockers to be used and observes its state.
  void AddNotificationBlocker(std::unique_ptr<NotificationBlocker> blocker);

 private:
  // Removes a queued notification by its |notification_id| and returns if there
  // was a queued notification with that id. If |notify| is true this will
  // notify all relevant blockers about the removal.
  bool DoRemoveQueuedNotification(const std::string& notification_id,
                                  bool notify);

  // Called when the state of a notification blocker changes. Will display and
  // free all queued notifications if no blocker is active anymore.
  void MaybeDisplayQueuedNotifications();

  // Checks if any notification blocker is currently active for |notification|.
  bool IsAnyNotificationBlockerActive(
      const message_center::Notification& notification) const;

  // Represents a queued notification.
  struct QueuedNotification {
    QueuedNotification(NotificationHandler::Type notification_type,
                       const message_center::Notification& notification,
                       std::unique_ptr<NotificationCommon::Metadata> metadata);
    QueuedNotification(QueuedNotification&&);
    QueuedNotification& operator=(QueuedNotification&&);
    ~QueuedNotification();

    NotificationHandler::Type notification_type;
    message_center::Notification notification;
    std::unique_ptr<NotificationCommon::Metadata> metadata;
  };

  // The |notification_display_service_| owns |this|.
  raw_ptr<NotificationDisplayService> notification_display_service_;

  // A list of notification blockers that indicate when notifications should be
  // blocked and notify when their state changes.
  NotificationBlockers blockers_;

  // A list of queued notifications in order of last update. Each notification
  // has a unique id in this list as adding a duplication will remove the
  // existing one before inserting at the end.
  std::vector<QueuedNotification> queued_notifications_;

  // Observer for the list of |blockers_|.
  base::ScopedMultiSourceObservation<NotificationBlocker,
                                     NotificationBlocker::Observer>
      notification_blocker_observations_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_QUEUE_H_
