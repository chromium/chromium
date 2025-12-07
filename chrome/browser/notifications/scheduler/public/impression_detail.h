// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace notifications {

struct ImpressionDetail {
  using ImpressionDetailCallback = base::OnceCallback<void(ImpressionDetail)>;

  ImpressionDetail();
  ImpressionDetail(size_t current_max_daily_show,
                   size_t num_shown_today,
                   size_t num_negative_events,
                   std::optional<base::Time> last_negative_event_ts,
                   std::optional<base::Time> last_shown_ts);

  bool operator==(const ImpressionDetail& other) const;

  // The maximum number of notifications shown to the user for this type.
  // May change if the user interacts with the notification per day.
  size_t current_max_daily_show = 0;

  // The current number of notifications shown to the user for this type today.
  size_t num_shown_today = 0;

  // The number of negative events caused by concecutive dismiss or not
  // helpful button clicking in all time.
  // Persisted in protodb.
  size_t num_negative_events = 0;

  // Timestamp of last negative event.
  // Persisted in protodb.
  std::optional<base::Time> last_negative_event_ts;

  // Timestamp of last shown notification.
  // Persisted in protodb.
  std::optional<base::Time> last_shown_ts;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_
