// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_TYPES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_TYPES_H_

#include <map>
#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Contains data to determine when a notification should be shown to the user
// and the user impression towards this notification.
//
// Life cycle:
// 1. Created after the notification is shown to the user.
// 2. |feedback| is set after the user interacts with the notification.
// 3. Notification scheduler API consumer gets the user feedback and generates
// an impression result, which may affect notification exposure.
// 4. The impression is deleted after it expires.
struct Impression {
  using ImpressionResultMap = std::map<UserFeedback, ImpressionResult>;
  using CustomData = std::map<std::string, std::string>;

  Impression();
  Impression(SchedulerClientType type,
             const std::string& guid,
             const base::Time& create_time);
  Impression(const Impression& other);
  Impression(Impression&& other);
  Impression& operator=(const Impression& other);
  Impression& operator=(Impression&& other);
  ~Impression();

  bool operator==(const Impression& other) const;

  // Creation timestamp.
  base::Time create_time;

  // The user feedback on the notification, each notification will have at most
  // one feedback. Sets after the user interacts with the notification.
  UserFeedback feedback = UserFeedback::kNoFeedback;

  // The impression type. The client of a notification type takes one or several
  // user feedbacks as input and generate a user impression, which will
  // eventually affect the rate to deliver notifications to the user.
  ImpressionResult impression = ImpressionResult::kInvalid;

  // If the user feedback is used in computing the current notification deliver
  // rate.
  bool integrated = false;

  // The unique identifier of the notification.
  std::string guid;

  // The type of the notification. Not persisted to disk, set after database
  // initialized.
  // TODO(xingliu): Consider to persist this as well.
  SchedulerClientType type = SchedulerClientType::kUnknown;

  // Used to override default impression result.
  ImpressionResultMap impression_mapping;

  // Custom data associated with a notification. Send back to the client when
  // the user interacts with the notification.
  CustomData custom_data;

  // Duration to mark a notification without feedback as ignored.
  std::optional<base::TimeDelta> ignore_timeout_duration;
};

// Contains details about supression and recovery after suppression expired.
struct SuppressionInfo {
  SuppressionInfo(const base::Time& last_trigger,
                  const base::TimeDelta& duration);
  SuppressionInfo(const SuppressionInfo& other);
  ~SuppressionInfo() = default;
  bool operator==(const SuppressionInfo& other) const;

  // Time that the suppression should release.
  base::Time ReleaseTime() const;

  // The last supression trigger time.
  base::Time last_trigger_time;

  // The duration for the suppression.
  base::TimeDelta duration;

  // |current_max_daily_show| will change to this after the suppression
  // expired.
  int recover_goal;
};

// Stores the global states about how often the notification can be shown
// to the user and the history of user interactions to a particular notification
// client.
struct ClientState {
  using Impressions = base::circular_deque<Impression>;

  ClientState();
  ClientState(const ClientState& other);
  ClientState(ClientState&& other);
  ClientState& operator=(const ClientState& other);
  ClientState& operator=(ClientState&& other);

  ~ClientState();

  bool operator==(const ClientState& other) const;

  // The type of notification using the scheduler.
  SchedulerClientType type;

  // The maximum number of notifications shown to the user for this type. May
  // change if the user interacts with the notification.
  int current_max_daily_show;

  // A list of user impression history. Sorted by creation time.
  Impressions impressions;

  // Suppression details, no value if there is currently no suppression.
  std::optional<SuppressionInfo> suppression_info;

  // The number of negative events caused by concecutive dismiss or not helpful
  // button clicking in all time. Persisted in protodb.
  size_t negative_events_count;

  // Timestamp of last negative event occurred. Persisted in protodb.
  std::optional<base::Time> last_negative_event_ts;

  // Timestamp of last shown notification.
  // Persisted in protodb.
  std::optional<base::Time> last_shown_ts;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_TYPES_H_
