// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace message_center {
class Notification;
}

// Profile-bound service that enables user-visible notifications to be displayed
// and managed. Notifications may either be presented using a notification
// center provided by the platform, or by Chrome's Message Center.
class NotificationDisplayService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the |notification| is displayed. The |metadata| is provided
    // for persistent web page notifications only, which require
    // |service_worker_scope|.
    virtual void OnNotificationDisplayed(
        const message_center::Notification& notification,
        const NotificationCommon::Metadata* const metadata) = 0;

    // Invoked when the notification having |notification_id| is closed.
    virtual void OnNotificationClosed(const std::string& notification_id) = 0;

    // Invoked when the NotificationDisplayService object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "RemoveObserver(this)", whether directly or indirectly (e.g.
    // via ScopedObserver::Remove).
    virtual void OnNotificationDisplayServiceDestroyed(
        NotificationDisplayService* service) = 0;

   protected:
    ~Observer() override;
  };

  NotificationDisplayService(const NotificationDisplayService&) = delete;
  NotificationDisplayService& operator=(const NotificationDisplayService&) =
      delete;
  ~NotificationDisplayService() override;

  // Callback to be used with the GetDisplayed() method. Includes the set of
  // notification ids that is being displayed to the user. The
  // |supports_synchronization| callback indicates whether the platform has the
  // ability to query which notifications are still being displayed.
  //
  // TODO(peter): Rename |supports_synchronization| to |supported|.
  using DisplayedNotificationsCallback =
      base::OnceCallback<void(std::set<std::string>,
                              bool /* supports_synchronization */)>;

  // Displays the |notification| of type |notification_type|. The |metadata|
  // may be provided for certain notification types that require additional
  // information for the notification to be displayed.
  virtual void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) = 0;

  // Closes the notification having |notification_id| of |notification_type|.
  virtual void Close(NotificationHandler::Type notification_type,
                     const std::string& notification_id) = 0;

  // Gets the IDs of currently displaying notifications and invokes |callback|
  // once available. Not all backends support retrieving this information.
  // TODO(crbug.com/40283098): Consider refactoring this API and its
  // usage to something that can get implemented by more backends.
  virtual void GetDisplayed(DisplayedNotificationsCallback callback) = 0;

  // Gets the IDs of currently displaying notifications associated with `origin`
  // and invokes `callback` once available. Not all backends support retrieving
  // this information.
  virtual void GetDisplayedForOrigin(
      const GURL& origin,
      DisplayedNotificationsCallback callback) = 0;

  // Adds and removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  NotificationDisplayService() = default;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
