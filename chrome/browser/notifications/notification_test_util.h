// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "ui/message_center/public/cpp/notification.h"

class Browser;
class Profile;

class StubNotificationUIManager : public NotificationUIManager {
 public:
  StubNotificationUIManager();
  ~StubNotificationUIManager() override;

  // Returns the number of currently active notifications.
  unsigned int GetNotificationCount() const;

  // Returns a reference to the notification at index |index|.
  const message_center::Notification& GetNotificationAt(
      unsigned int index) const;

  // Sets a one-shot callback that will be invoked when a notification has been
  // added to the Notification UI manager. Will be invoked on the UI thread.
  void SetNotificationAddedCallback(const base::Closure& callback);

  // Emulates clearing a notification from the notification center
  // without running any of the delegates. This may happen when native
  // notification centers don't inform us about closed notifications,
  // for example as a result of a system reboot.
  bool SilentDismissById(const std::string& delegate_id, ProfileID profile_id);

  // NotificationUIManager implementation.
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

  GURL& last_canceled_source() { return last_canceled_source_; }

 private:
  using NotificationPair = std::pair<message_center::Notification, ProfileID>;
  std::vector<NotificationPair> notifications_;

  base::Closure notification_added_callback_;

  bool is_shutdown_started_ = false;
  GURL last_canceled_source_;

  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
};

#if !defined(OS_ANDROID)
// Helper class that has to be created in the stack to check if the fullscreen
// setting of a browser is in the desired state.
class FullscreenStateWaiter {
 public:
  FullscreenStateWaiter(Browser* browser, bool desired_state);

  void Wait();

 private:
  Browser* const browser_;
  bool desired_state_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenStateWaiter);
};
#endif

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
