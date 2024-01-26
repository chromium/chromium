// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

struct NotificationData;
struct NotificationEntry;

namespace test {

// Flattened type state data used in test to generate client states.
struct ImpressionTestData {
  ImpressionTestData(SchedulerClientType type,
                     size_t current_max_daily_show,
                     std::vector<Impression> impressions,
                     std::optional<SuppressionInfo> suppression_info,
                     size_t negative_events_count,
                     std::optional<base::Time> last_negative_event_ts,
                     std::optional<base::Time> last_shown_ts);

  ImpressionTestData(const ImpressionTestData& other);
  ~ImpressionTestData();

  SchedulerClientType type;
  size_t current_max_daily_show;
  std::vector<Impression> impressions;
  std::optional<SuppressionInfo> suppression_info;
  size_t negative_events_count;
  std::optional<base::Time> last_negative_event_ts;
  std::optional<base::Time> last_shown_ts;
};

// Add one impression test data into a client state.
void AddImpressionTestData(const ImpressionTestData& data,
                           ClientState* client_state);

// Adds impression test data into client states container.
void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    ImpressionHistoryTracker::ClientStates* client_states);

// Adds impression test data into client states container.
void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    std::vector<std::unique_ptr<ClientState>>* client_states);

// Creates an impression.
Impression CreateImpression(const base::Time& create_time,
                            UserFeedback feedback,
                            ImpressionResult impression,
                            bool integrated,
                            const std::string& guid,
                            SchedulerClientType type);

// Generates a debug string to print details of |data|.
std::string DebugString(const NotificationData* data);

// Generates a debug string to print details of |entry|.
std::string DebugString(const NotificationEntry* entry);

// Generates a debug string to print details of |client_state|.
std::string DebugString(const ClientState* client_state);

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_
