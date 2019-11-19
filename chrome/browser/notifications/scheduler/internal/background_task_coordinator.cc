// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"

namespace notifications {
namespace {

class BackgroundTaskCoordinatorHelper {
 public:
  BackgroundTaskCoordinatorHelper(
      NotificationBackgroundTaskScheduler* background_task,
      const SchedulerConfig* config,
      base::Clock* clock)
      : background_task_(background_task), config_(config), clock_(clock) {}
  ~BackgroundTaskCoordinatorHelper() = default;

  void ScheduleBackgroundTask(
      BackgroundTaskCoordinator::Notifications notifications,
      BackgroundTaskCoordinator::ClientStates client_states) {
    if (notifications.empty()) {
      background_task_->Cancel();
      return;
    }

    BackgroundTaskCoordinator::Notifications unthrottled_notifications;
    BackgroundTaskCoordinator::Notifications throttled_notifications;
    for (auto& pair : notifications) {
      for (auto* notification : pair.second) {
        auto type = pair.first;
        if (notification->schedule_params.priority ==
            ScheduleParams::Priority::kNoThrottle) {
          unthrottled_notifications[type].emplace_back(std::move(notification));
        } else {
          throttled_notifications[type].emplace_back(std::move(notification));
        }
      }
    }
    ProcessUnthrottledNotifications(std::move(unthrottled_notifications));
    ProcessThrottledNotifications(std::move(throttled_notifications),
                                  std::move(client_states));
    ScheduleBackgroundTaskInternal();
  }

 private:
  void ProcessUnthrottledNotifications(
      BackgroundTaskCoordinator::Notifications notifications) {
    for (const auto& pair : notifications) {
      for (const auto* entry : pair.second) {
        DCHECK_EQ(entry->schedule_params.priority,
                  ScheduleParams::Priority::kNoThrottle);
        if (!entry->schedule_params.deliver_time_start.has_value()) {
          continue;
        }
        base::Time deliver_time_start =
            entry->schedule_params.deliver_time_start.value();
        MaybeUpdateBackgroundTaskTime(deliver_time_start);
      }
    }
  }

  void ProcessThrottledNotifications(
      BackgroundTaskCoordinator::Notifications notifications,
      BackgroundTaskCoordinator::ClientStates client_states) {
    base::Time tomorrow;
    base::Time now = clock_->Now();
    bool success = ToLocalHour(0, now, 1 /*day_delta*/, &tomorrow);
    DCHECK(success);

    std::map<SchedulerClientType, int> shown_per_type;
    int shown_total = 0;
    SchedulerClientType last_shown_type = SchedulerClientType::kUnknown;
    NotificationsShownToday(client_states, &shown_per_type, &shown_total,
                            &last_shown_type, clock_);

    for (const auto& pair : notifications) {
      auto type = pair.first;
      auto it = client_states.find(type);
      if (pair.second.empty() || (it == client_states.end()))
        continue;

      const ClientState* client_state = it->second;
      bool reach_max_today =
          shown_per_type[type] >= client_state->current_max_daily_show ||
          shown_total >= config_->max_daily_shown_all_type;

      // Find the eariliest notification to launch the background task.
      for (const auto* entry : pair.second) {
        DCHECK_NE(entry->schedule_params.priority,
                  ScheduleParams::Priority::kNoThrottle);
        // Currently only support deliver time window.
        if (!entry->schedule_params.deliver_time_start.has_value()) {
          continue;
        }

        base::Time deliver_time_start =
            entry->schedule_params.deliver_time_start.value();

        // Consider suppression time.
        if (client_state->suppression_info.has_value() &&
            deliver_time_start <
                client_state->suppression_info->ReleaseTime()) {
          deliver_time_start = client_state->suppression_info->ReleaseTime();
        }

        // Consider daily limit throttling.
        if (reach_max_today && deliver_time_start < tomorrow)
          deliver_time_start = tomorrow;

        // Deliver time window has passed.
        DCHECK(entry->schedule_params.deliver_time_end.has_value());
        if (!entry->schedule_params.deliver_time_end.has_value() ||
            deliver_time_start >
                entry->schedule_params.deliver_time_end.value()) {
          continue;
        }

        MaybeUpdateBackgroundTaskTime(deliver_time_start);
      }
    }
  }

  void MaybeUpdateBackgroundTaskTime(const base::Time& time) {
    if (!background_task_time_.has_value() ||
        time < background_task_time_.value())
      background_task_time_ = time;
  }

  void ScheduleBackgroundTaskInternal() {
    if (!background_task_time_.has_value())
      return;

    base::TimeDelta window_start_time =
        background_task_time_.value() - clock_->Now();
    window_start_time = base::ClampToRange(window_start_time, base::TimeDelta(),
                                           base::TimeDelta::Max());

    // TODO(xingliu): Remove SchedulerTaskTime.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kNotificationSchedulerImmediateBackgroundTask)) {
      background_task_->Schedule(
          base::TimeDelta(),
          base::TimeDelta() + base::TimeDelta::FromMinutes(1));
      return;
    }

    background_task_->Schedule(
        window_start_time,
        window_start_time + config_->background_task_window_duration);
  }

  NotificationBackgroundTaskScheduler* background_task_;
  const SchedulerConfig* config_;
  base::Clock* clock_;
  base::Optional<base::Time> background_task_time_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinatorHelper);
};

}  // namespace

class BackgroundTaskCoordinatorImpl : public BackgroundTaskCoordinator {
 public:
  BackgroundTaskCoordinatorImpl(
      std::unique_ptr<NotificationBackgroundTaskScheduler> background_task,
      const SchedulerConfig* config,
      base::Clock* clock)
      : background_task_(std::move(background_task)),
        config_(config),
        clock_(clock) {}

  ~BackgroundTaskCoordinatorImpl() override = default;

 private:
  // BackgroundTaskCoordinator implementation.
  void ScheduleBackgroundTask(Notifications notifications,
                              ClientStates client_states) override {
    auto helper = std::make_unique<BackgroundTaskCoordinatorHelper>(
        background_task_.get(), config_, clock_);
    helper->ScheduleBackgroundTask(std::move(notifications),
                                   std::move(client_states));
  }

  // The class that actually schedules platform background task.
  std::unique_ptr<NotificationBackgroundTaskScheduler> background_task_;

  // System configuration.
  const SchedulerConfig* config_;

  // Clock to query the current timestamp.
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinatorImpl);
};

// static
std::unique_ptr<BackgroundTaskCoordinator> BackgroundTaskCoordinator::Create(
    std::unique_ptr<NotificationBackgroundTaskScheduler> background_task,
    const SchedulerConfig* config,
    base::Clock* clock) {
  return std::make_unique<BackgroundTaskCoordinatorImpl>(
      std::move(background_task), config, clock);
}

BackgroundTaskCoordinator::~BackgroundTaskCoordinator() = default;

}  // namespace notifications
