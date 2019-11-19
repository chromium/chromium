// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_system_observer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification.h"

class PopupsOnlyUiController;
class Profile;
class ProfileNotification;

namespace message_center {
class Notification;
class NotificationBlocker;
FORWARD_DECLARE_TEST(NotificationTrayTest, ManuallyCloseMessageCenter);
}  // namespace message_center

// This class extends NotificationUIManager and delegates actual display
// of notifications to MessageCenter, doing necessary conversions. This is only
// used on platforms that support non-native notifications.
class NotificationUIManagerImpl : public NotificationUIManager,
                                  public message_center::MessageCenterObserver,
                                  public ProfileObserver {
 public:
  NotificationUIManagerImpl();
  ~NotificationUIManagerImpl() override;

  // NotificationUIManager
  void Add(const message_center::Notification& notification,
           Profile* profile) override;
  bool Update(const message_center::Notification& notification,
              Profile* profile) override;
  const message_center::Notification* FindById(
      const std::string& delegate_id,
      ProfileID profile_id) const override;
  bool CancelById(const std::string& delegate_id,
                  ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfile(ProfileID profile_id) override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  void CancelAll() override;
  void StartShutdown() override;

  // MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Resets the ui controller.
  void ResetUiControllerForTest();

  // Returns the notification id which this manager will use to add to message
  // center, for this combination of delegate id and profile.
  std::string GetMessageCenterNotificationIdForTest(
      const std::string& delegate_id,
      Profile* profile);

  // Returns true if the popup bubbles are currently visible.
  bool popups_visible() const { return popups_visible_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(message_center::NotificationTrayTest,
                           ManuallyCloseMessageCenter);

  // Use a map by notification_id since this mapping is the most often used.
  std::map<std::string, std::unique_ptr<ProfileNotification>>
      profile_notifications_;

  // Helpers that add/remove the notification from local map.
  // The local map takes ownership of profile_notification object.
  void AddProfileNotification(
      std::unique_ptr<ProfileNotification> profile_notification);
  void RemoveProfileNotification(const std::string& notification_id);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  // To own the blockers.
  std::vector<std::unique_ptr<message_center::NotificationBlocker>> blockers_;

  NotificationSystemObserver system_observer_;

  ScopedObserver<Profile, ProfileObserver> observed_otr_profiles_{this};

  // Delegate of this class.
  std::unique_ptr<PopupsOnlyUiController> popups_only_ui_controller_;

  // Tracks if shutdown has started.
  bool is_shutdown_started_ = false;

  // Tracks the current visibility status of the popup bubbles.
  bool popups_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_IMPL_H_
