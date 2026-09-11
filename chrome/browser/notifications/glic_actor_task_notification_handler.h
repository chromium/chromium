// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_

#include <string>

#include "chrome/browser/notifications/notification_handler.h"
#include "components/actor/core/task_id.h"

class Profile;

class GlicActorTaskNotificationHandler : public NotificationHandler {
 public:
  GlicActorTaskNotificationHandler();
  ~GlicActorTaskNotificationHandler() override;

  GlicActorTaskNotificationHandler(const GlicActorTaskNotificationHandler&) =
      delete;
  GlicActorTaskNotificationHandler& operator=(
      const GlicActorTaskNotificationHandler&) = delete;

  // Shows an OS-level notification when an experimental task starts if the
  // feature is enabled and no browser window is active.
  static void MaybeShow(Profile* profile, actor::TaskId task_id);

  // Closes the OS-level notification for the task if it is showing.
  static void Close(Profile* profile, actor::TaskId task_id);

  // NotificationHandler:
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_
