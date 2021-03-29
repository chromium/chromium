// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "url/gurl.h"

class Profile;

// Handles NOTIFICATIONS_MUTED notification actions.
class MutedNotificationHandler : public NotificationHandler {
 public:
  // Actions taken by the user on a muted notification.
  enum class Action {
    // The user explicitly closed the notification.
    kUserClose,
    // The user clicked on the notification body.
    kBodyClick,
    // The user clicked on the "Show" action button.
    kShowClick,
  };

  // Delegate for handling muted notification actions.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the user performed an |action| on a muted notification.
    virtual void OnAction(Action action) = 0;
  };

  explicit MutedNotificationHandler(Delegate* delegate);
  MutedNotificationHandler(const MutedNotificationHandler&) = delete;
  MutedNotificationHandler& operator=(const MutedNotificationHandler&) = delete;
  ~MutedNotificationHandler() override;

  // NotificationHandler:
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

 private:
  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_
