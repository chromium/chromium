// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"

class GURL;
class NotificationPlatformBridge;
class Profile;

// Implementation of the NotificationDisplayService interface. Methods that are
// not available in the base interface should only be used by the platform
// notification bridges.
class NotificationDisplayServiceImpl : public NotificationDisplayService {
 public:
  // Note that |profile| might be nullptr for notification display service used
  // for system notifications. The system instance is owned by
  // SystemNotificationHelper, and is only expected to handle TRANSIENT
  // notifications.
  explicit NotificationDisplayServiceImpl(Profile* profile);
  ~NotificationDisplayServiceImpl() override;

  // Returns an instance of the display service implementation for the given
  // |profile|. This should be removed in favor of multiple statics for handling
  // the individual notification operations.
  static NotificationDisplayServiceImpl* GetForProfile(Profile* profile);

  // Used to propagate back events originate from the user. The events are
  // received and dispatched to the right consumer depending on the type of
  // notification. Consumers include, service workers, pages, extensions...
  //
  // TODO(peter): Remove this in favor of multiple targetted methods.
  virtual void ProcessNotificationOperation(
      NotificationCommon::Operation operation,
      NotificationHandler::Type notification_type,
      const GURL& origin,
      const std::string& notification_id,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply,
      const base::Optional<bool>& by_user);

  // Registers an implementation object to handle notification operations
  // for |notification_type|.
  void AddNotificationHandler(NotificationHandler::Type notification_type,
                              std::unique_ptr<NotificationHandler> handler);

  // Returns the notification handler that was registered for the given type.
  // May return null.
  NotificationHandler* GetNotificationHandler(
      NotificationHandler::Type notification_type);

  // NotificationDisplayService implementation:
  void Shutdown() override;
  void Display(NotificationHandler::Type notification_type,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(DisplayedNotificationsCallback callback) override;

  static void ProfileLoadedCallback(NotificationCommon::Operation operation,
                                    NotificationHandler::Type notification_type,
                                    const GURL& origin,
                                    const std::string& notification_id,
                                    const base::Optional<int>& action_index,
                                    const base::Optional<base::string16>& reply,
                                    const base::Optional<bool>& by_user,
                                    Profile* profile);

 private:
  // Called when the NotificationPlatformBridge may have been initialized.
  void OnNotificationPlatformBridgeReady(bool success);

  Profile* profile_;

  // Bridge responsible for displaying notifications on the platform. The
  // message center's bridge is maintained for platforms where it is available.
  std::unique_ptr<NotificationPlatformBridge> message_center_bridge_;
  NotificationPlatformBridge* bridge_;

  // Tasks that need to be run once the display bridge has been initialized.
  base::queue<base::OnceClosure> actions_;

  // Boolean tracking whether the |bridge_| has been initialized for use.
  bool bridge_initialized_ = false;

  // Map containing the notification handlers responsible for processing events.
  std::map<NotificationHandler::Type, std::unique_ptr<NotificationHandler>>
      notification_handlers_;

  base::WeakPtrFactory<NotificationDisplayServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayServiceImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
