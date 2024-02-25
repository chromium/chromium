// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_queue.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegator.h"
#include "chrome/common/notifications/notification_operation.h"

class GURL;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

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
  NotificationDisplayServiceImpl(const NotificationDisplayServiceImpl&) =
      delete;
  NotificationDisplayServiceImpl& operator=(
      const NotificationDisplayServiceImpl&) = delete;
  ~NotificationDisplayServiceImpl() override;

  // Returns an instance of the display service implementation for the given
  // |profile|. This should be removed in favor of multiple statics for handling
  // the individual notification operations.
  static NotificationDisplayServiceImpl* GetForProfile(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Used to propagate back events originate from the user. The events are
  // received and dispatched to the right consumer depending on the type of
  // notification. Consumers include, service workers, pages, extensions...
  //
  // TODO(peter): Remove this in favor of multiple targeted methods.
  virtual void ProcessNotificationOperation(
      NotificationOperation operation,
      NotificationHandler::Type notification_type,
      const GURL& origin,
      const std::string& notification_id,
      const std::optional<int>& action_index,
      const std::optional<std::u16string>& reply,
      const std::optional<bool>& by_user);

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
  void GetDisplayedForOrigin(const GURL& origin,
                             DisplayedNotificationsCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  static void ProfileLoadedCallback(NotificationOperation operation,
                                    NotificationHandler::Type notification_type,
                                    const GURL& origin,
                                    const std::string& notification_id,
                                    const std::optional<int>& action_index,
                                    const std::optional<std::u16string>& reply,
                                    const std::optional<bool>& by_user,
                                    Profile* profile);

  // Sets the list of |blockers| to be used by the |notification_queue_|. Only
  // used in tests.
  void SetBlockersForTesting(
      NotificationDisplayQueue::NotificationBlockers blockers);

  // Sets the platform bridge delegator for tests.
  void SetNotificationPlatformBridgeDelegatorForTesting(
      std::unique_ptr<NotificationPlatformBridgeDelegator> bridge_delegator);

  // Sets an implementation object to handle notification operations for
  // |notification_type| and overrides any existing ones.
  void OverrideNotificationHandlerForTesting(
      NotificationHandler::Type notification_type,
      std::unique_ptr<NotificationHandler> handler);

 private:
  // Called when the NotificationPlatformBridgeDelegator has been initialized.
  void OnNotificationPlatformBridgeReady();

  // Called after getting displayed notifications from the bridge so we can add
  // any currently queued notification ids. If `origin` is set, we only want to
  // get the notifications associated with that origin.
  void OnGetDisplayed(std::optional<GURL> origin,
                      DisplayedNotificationsCallback callback,
                      std::set<std::string> notification_ids,
                      bool supports_synchronization);

  raw_ptr<Profile> profile_;

  // This NotificationPlatformBridgeDelegator delegates to either the native
  // bridge or to the MessageCenter if there is no native bridge or it does not
  // support certain notification types.
  std::unique_ptr<NotificationPlatformBridgeDelegator> bridge_delegator_;

  // Tasks that need to be run once the display bridge has been initialized.
  base::queue<base::OnceClosure> actions_;

  // Boolean tracking whether the |bridge_delegator_| has been initialized.
  bool bridge_delegator_initialized_ = false;

  // Notification queue that holds on to notifications instead of displaying
  // them if certain blockers are temporarily active.
  NotificationDisplayQueue notification_queue_{this};

  // Map containing the notification handlers responsible for processing events.
  std::map<NotificationHandler::Type, std::unique_ptr<NotificationHandler>>
      notification_handlers_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<NotificationDisplayServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
