// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_UTILS_H_

#include <map>
#include <memory>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

struct ClientState;
struct SchedulerConfig;

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
