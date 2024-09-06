// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/display_decider.h"

#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/time/clock.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

using Notifications = notifications::DisplayDecider::Notifications;
using Results = notifications::DisplayDecider::Results;
using ClientStates = notifications::DisplayDecider::ClientStates;

namespace notifications {
namespace {

// Helper class contains the actual logic to decide which notifications to show.
// This is an one-shot class, callers should create a new object each time.
class DecisionHelper {
 public:
  DecisionHelper(const SchedulerConfig* config,
                 const std::vector<SchedulerClientType>& clients,
                 base::Clock* clock,
                 Notifications notifications,
                 ClientStates client_states)
      : notifications_(std::move(notifications)),
        client_states_(std::move(client_states)),
        config_(config),
        clients_(clients),
        clock_(clock),
        last_shown_type_(SchedulerClientType::kUnknown),
        shown_(0) {}

  DecisionHelper(const DecisionHelper&) = delete;
  DecisionHelper& operator=(const DecisionHelper&) = delete;
  ~DecisionHelper() = default;

  // Figures out a list of notifications to show.
  void DecideNotificationToShow(Results* results) {
    NotificationsShownToday(client_states_, &shown_per_type_, &shown_,
                            &last_shown_type_, clock_);
    FilterNotifications();
    PickNotificationToShow(results);
  }

 private:
  // Filter notifications based on scheduling parameters.
  void FilterNotifications() {
    Notifications filtered_notifications;
    for (const auto& pair : notifications_) {
      // Client under suppression will not have notification to show.
      auto it = client_states_.find(pair.first);
      if (it != client_states_.end() &&
          it->second->suppression_info.has_value()) {
        continue;
      }

      for (const notifications::NotificationEntry* notification : pair.second) {
        DCHECK(notification);
        DCHECK_NE(notification->schedule_params.priority,
                  ScheduleParams::Priority::kNoThrottle);
        if (!ShouldFilterOut(notification))
          filtered_notifications[notification->type].emplace_back(notification);
      }
    }

    notifications_.swap(filtered_notifications);
  }

  bool ShouldFilterOut(const NotificationEntry* entry) {
    base::Time now = clock_->Now();
    // Filter with time window. Must have |deliver_time_start|.
    if (!entry->schedule_params.deliver_time_start.has_value())
      return true;
    bool meet_deliver_time_start =
        now >= entry->schedule_params.deliver_time_start.value();

    DCHECK(entry->schedule_params.deliver_time_end.has_value());
    bool meet_deliver_time_end =
        entry->schedule_params.deliver_time_end.has_value()
            ? now <= entry->schedule_params.deliver_time_end.value()
            : false;
    if (meet_deliver_time_start && meet_deliver_time_end) {
      return false;
    }

    return true;
  }

  // Picks a list of notifications to show.
  void PickNotificationToShow(Results* to_show) {
    DCHECK(to_show);
    if (shown_ > config_->max_daily_shown_all_type || clients_.empty())
      return;

    // No previous shown notification, move the iterator to last element.
    // We will iterate through all client types later.
    auto it = base::ranges::find(clients_, last_shown_type_);
    if (it == clients_.end()) {
      DCHECK_EQ(last_shown_type_, SchedulerClientType::kUnknown);
      last_shown_type_ = clients_.back();
      it = clients_.end() - 1;
    }

    DCHECK_NE(last_shown_type_, SchedulerClientType::kUnknown);
    size_t steps = 0u;

    // Circling around all clients to find new notification to show.
    do {
      // Move the iterator to next client type.
      CHECK(it != clients_.end(), base::NotFatalUntil::M130);
      if (++it == clients_.end())
        it = clients_.begin();
      ++steps;

      SchedulerClientType type = *it;

      // Check quota for all types and current background task type.
      if (ReachDailyQuota())
        break;

      // Check quota for this type, and continue to iterate other types.
      if (NoMoreNotificationToShow(type))
        continue;

      // Show the last notification in the vector. Notice the order depends on
      // how the vector is sorted.
      to_show->emplace(notifications_[type].back()->guid);
      notifications_[type].pop_back();
      shown_per_type_[type]++;
      shown_++;
      steps = 0u;

      // Stop if we didn't find anything new to show, and have looped around
      // all clients.
    } while (steps <= clients_.size());
  }

  bool NoMoreNotificationToShow(SchedulerClientType type) {
    auto it = client_states_.find(type);
    int max_daily_show =
        it == client_states_.end() ? 0 : it->second->current_max_daily_show;

    return notifications_[type].empty() ||
           shown_per_type_[type] >= config_->max_daily_shown_per_type ||
           shown_per_type_[type] >= max_daily_show;
  }

  bool ReachDailyQuota() const {
    return shown_ >= config_->max_daily_shown_all_type;
  }

  // Scheduled notifications as candidates to display to the user.
  Notifications notifications_;

  const ClientStates client_states_;
  raw_ptr<const SchedulerConfig, DanglingUntriaged> config_;
  const std::vector<SchedulerClientType> clients_;
  raw_ptr<base::Clock> clock_;

  SchedulerClientType last_shown_type_;
  std::map<SchedulerClientType, int> shown_per_type_;
  int shown_;
};

class DisplayDeciderImpl : public DisplayDecider {
 public:
  DisplayDeciderImpl(const SchedulerConfig* config,
                     std::vector<SchedulerClientType> clients,
                     base::Clock* clock)
      : config_(config), clients_(std::move(clients)), clock_(clock) {}
  DisplayDeciderImpl(const DisplayDeciderImpl&) = delete;
  DisplayDeciderImpl& operator=(const DisplayDeciderImpl&) = delete;
  ~DisplayDeciderImpl() override = default;

 private:
  // DisplayDecider implementation.
  void FindNotificationsToShow(Notifications notifications,
                               ClientStates client_states,
                               Results* results) override {
    Notifications throttled_notifications;
    for (const auto& pair : notifications) {
      auto type = pair.first;
      for (const notifications::NotificationEntry* notification : pair.second) {
        // Move unthrottled notifications to results directly.
        if (notification->schedule_params.priority ==
            ScheduleParams::Priority::kNoThrottle) {
          results->emplace(notification->guid);
        } else {
          throttled_notifications[type].emplace_back(notification);
        }
      }
    }
    // Handle throttled notifications.
    auto helper = std::make_unique<DecisionHelper>(
        config_, clients_, clock_, std::move(throttled_notifications),
        std::move(client_states));
    helper->DecideNotificationToShow(results);
  }

  raw_ptr<const SchedulerConfig, DanglingUntriaged> config_;
  const std::vector<SchedulerClientType> clients_;
  raw_ptr<base::Clock> clock_;
};

}  // namespace

// static
std::unique_ptr<DisplayDecider> DisplayDecider::Create(
    const SchedulerConfig* config,
    std::vector<SchedulerClientType> clients,
    base::Clock* clock) {
  return std::make_unique<DisplayDeciderImpl>(config, std::move(clients),
                                              clock);
}

}  // namespace notifications
