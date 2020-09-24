// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_BLOCKER_H_

#include "base/observer_list.h"

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
  // notifications right now.
  virtual bool ShouldBlockNotifications() = 0;

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
