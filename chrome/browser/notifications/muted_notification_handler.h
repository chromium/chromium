// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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
    // The user clicked on the "Snooze" action button.
    kSnoozeClick,
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
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

  Delegate* get_delegate_for_testing() const { return delegate_; }

 private:
  raw_ptr<Delegate, DanglingUntriaged> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MUTED_NOTIFICATION_HANDLER_H_
