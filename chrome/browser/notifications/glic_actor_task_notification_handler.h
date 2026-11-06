// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "components/actor/core/task_id.h"

class Profile;

// Recorded to UMA when a desktop OS notification for an experimental actor task
// is shown, skipped, clicked, or closed.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(GlicActorTaskNotificationEvent)
enum class GlicActorTaskNotificationEvent {
  kShown = 0,
  kSkippedBrowserActive = 1,
  kClicked = 2,
  kClosed = 3,
  kMaxValue = kClosed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicActorTaskNotificationEvent)

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
  void OnShow(Profile* profile, const std::string& notification_id) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;

 private:
  // Tracks tasks whose OS notification is currently displayed so each shown
  // notification records at most one terminal click or close event (for
  // example, when `OnClick` or `Close` programmatically closes the notification
  // on MessageCenter).
  base::flat_set<actor::TaskId> displayed_tasks_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_GLIC_ACTOR_TASK_NOTIFICATION_HANDLER_H_
