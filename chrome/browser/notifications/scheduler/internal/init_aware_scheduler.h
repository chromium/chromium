// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_INIT_AWARE_SCHEDULER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_INIT_AWARE_SCHEDULER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

struct NotificationParams;

// NotificationScheduler implementation that caches NotificationScheduler API
// calls and flush them to the actual implementation after initialization
// succeeded.
class InitAwareNotificationScheduler : public NotificationScheduler {
 public:
  explicit InitAwareNotificationScheduler(
      std::unique_ptr<NotificationScheduler> impl);
  InitAwareNotificationScheduler(const InitAwareNotificationScheduler&) =
      delete;
  InitAwareNotificationScheduler& operator=(
      const InitAwareNotificationScheduler&) = delete;
  ~InitAwareNotificationScheduler() override;

 private:
  // NotificationScheduler implementation.
  void Init(InitCallback init_callback) override;
  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override;
  void DeleteAllNotifications(SchedulerClientType type) override;
  void GetClientOverview(
      SchedulerClientType type,
      ClientOverview::ClientOverviewCallback callback) override;
  void OnStartTask(TaskFinishedCallback callback) override;
  void OnStopTask() override;
  void OnUserAction(const UserActionData& action_data) override;

  // Called after initialization is done.
  void OnInitialized(InitCallback init_callback, bool success);

  // Whether initialization is successfully finished.
  bool IsReady() const;

  // Caches the closure if the initialization is not finished, or drops the
  // closure if initialization failed.
  void MaybeCacheClosure(base::OnceClosure closure);

  // Whether the initialization is successful. No value if initialization is not
  // finished.
  std::optional<bool> init_success_;

  // Cached calls.
  std::vector<base::OnceClosure> cached_closures_;

  // Actual implementation.
  std::unique_ptr<NotificationScheduler> impl_;

  base::WeakPtrFactory<InitAwareNotificationScheduler> weak_ptr_factory_{this};
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_INIT_AWARE_SCHEDULER_H_
