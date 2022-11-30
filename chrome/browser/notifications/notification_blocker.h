// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_BLOCKER_H_

#include "base/observer_list.h"

namespace message_center {
class Notification;
}  // namespace message_center

// Represents a notification blocker that prevents notifications from being
// displayed during certain times. These blockers work across all platforms and
// typically contain logic that the various OSs don't provide us with. The
// message center implementation for Chrome notifications has its own blockers
// which can be considered as separate OS level notification blockers like the
// ones that come with "Focus assist" on Windows.
class NotificationBlocker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the blocking state of this blocker has changed.
    virtual void OnBlockingStateChanged() = 0;
  };

  NotificationBlocker();
  NotificationBlocker(const NotificationBlocker&) = delete;
  NotificationBlocker& operator=(const NotificationBlocker&) = delete;
  virtual ~NotificationBlocker();

  // Implementations should return true if this blocker wants to block
  // |notification| right now.
  virtual bool ShouldBlockNotification(
      const message_center::Notification& notification) = 0;

  // Called when |notification| got blocked because this blocker is active.
  // |replaced| is true if the |notification| replaces a previously blocked one.
  virtual void OnBlockedNotification(
      const message_center::Notification& notification,
      bool replaced) {}

  // Called when a previously blocked |notification| got closed.
  virtual void OnClosedNotification(
      const message_center::Notification& notification) {}

  // Observer methods.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Implementations should call this whenever their blocking state changes.
  void NotifyBlockingStateChanged();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_BLOCKER_H_
