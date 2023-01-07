// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

#include <utility>

#include "base/containers/circular_deque.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "ui/gfx/codec/png_codec.h"

namespace notifications {

int NotificationsShownToday(const ClientState* state, base::Clock* clock) {
  std::map<SchedulerClientType, const ClientState*> client_states;
  std::map<SchedulerClientType, int> shown_per_type;
  client_states.emplace(state->type, state);
  int shown_total = 0;
  SchedulerClientType last_shown_type = SchedulerClientType::kUnknown;
  NotificationsShownToday(client_states, &shown_per_type, &shown_total,
                          &last_shown_type, clock);
  return shown_per_type[state->type];
}

void NotificationsShownToday(
    const std::map<SchedulerClientType, const ClientState*>& client_states,
    std::map<SchedulerClientType, int>* shown_per_type,
    int* shown_total,
    SchedulerClientType* last_shown_type,
    base::Clock* clock) {
  base::Time now = clock->Now();
  base::Time beginning_of_today;
  bool success = ToLocalHour(0, now, 0, &beginning_of_today);
  base::Time last_shown_time = beginning_of_today;
  DCHECK(success);
  for (const auto& state : client_states) {
    auto* client_state = state.second;
    int count = 0;
    for (const auto& impression : client_state->impressions) {
      if (impression.create_time >= beginning_of_today &&
          impression.create_time <= now) {
        count++;
        if (impression.create_time >= last_shown_time) {
          last_shown_time = impression.create_time;
          *last_shown_type = client_state->type;
        }
      }
    }
    (*shown_per_type)[client_state->type] = count;
    (*shown_total) += count;
  }
}

std::unique_ptr<ClientState> CreateNewClientState(
    SchedulerClientType type,
    const SchedulerConfig& config) {
  auto client_state = std::make_unique<ClientState>();
  client_state->type = type;
  client_state->current_max_daily_show = config.initial_daily_shown_per_type;
  return client_state;
}

}  // namespace notifications
