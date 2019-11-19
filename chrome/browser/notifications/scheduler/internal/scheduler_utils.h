// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_UTILS_H_

#include <map>
#include <memory>
#include <string>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

struct ClientState;
struct SchedulerConfig;

// Retrieves the time stamp of a certain hour at a certain day from today.
// |hour| must be in the range of [0, 23].
// |today| is a timestamp to define today, usually caller can directly pass in
// the current system time.
// |day_delta| is the different between the output date and today.
// Returns false if the conversion is failed.
bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out);

// Calculates the notifications shown today from impression data.
void NotificationsShownToday(
    const std::map<SchedulerClientType, const ClientState*>& client_states,
    std::map<SchedulerClientType, int>* shown_per_type,
    int* shown_total,
    SchedulerClientType* last_shown_type,
    base::Clock* clock = base::DefaultClock::GetInstance());

// Counts the number of notifications shown today of a given |state|.
int NotificationsShownToday(
    const ClientState* state,
    base::Clock* clock = base::DefaultClock::GetInstance());

// Creates client state data for new registered client.
std::unique_ptr<ClientState> CreateNewClientState(
    SchedulerClientType type,
    const SchedulerConfig& config);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_UTILS_H_
