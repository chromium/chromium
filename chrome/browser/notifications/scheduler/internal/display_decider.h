// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISPLAY_DECIDER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISPLAY_DECIDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace base {
class Clock;
}  // namespace base

namespace notifications {

struct ClientState;
struct NotificationEntry;
struct SchedulerConfig;

// This class uses scheduled notifications data and notification impression data
// of each notification type to find a list of notification that should be
// displayed to the user.
// All operations should be done on the main thread.
class DisplayDecider {
 public:
  using Notifications = std::map<
      SchedulerClientType,
      std::vector<raw_ptr<const NotificationEntry, VectorExperimental>>>;
  using ClientStates = std::map<SchedulerClientType, const ClientState*>;
  using Results = std::set<std::string>;

  // Creates the decider to determine notifications to show.
  static std::unique_ptr<DisplayDecider> Create(
      const SchedulerConfig* config,
      std::vector<SchedulerClientType> clients,
      base::Clock* clock);

  DisplayDecider() = default;
  DisplayDecider(const DisplayDecider&) = delete;
  DisplayDecider& operator=(const DisplayDecider&) = delete;
  virtual ~DisplayDecider() = default;

  // Finds notifications to show. Returns a list of notification guids.
  virtual void FindNotificationsToShow(
      Notifications notifications,
      ClientStates client_states,
      Results* results) = 0;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_DISPLAY_DECIDER_H_
