// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_THROTTLE_CONFIG_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_THROTTLE_CONFIG_H_

#include <optional>

#include "base/time/time.h"

namespace notifications {

// Specifies the throttling related configuration for each client.
struct ThrottleConfig {
  ThrottleConfig();
  ThrottleConfig(const ThrottleConfig& other);
  bool operator==(const ThrottleConfig& other) const;
  ~ThrottleConfig();

  // Support a custom suppression duration(in days) for the notification.
  // If client sets this field, it will override |suppression_duration| in
  // global config.
  std::optional<base::TimeDelta> suppression_duration;

  // Maxmium number of consecutive negative actions to trigger negative
  // impression event.
  // If client sets this field, it will override |dismiss_count| in global
  // config.
  std::optional<int> negative_action_count_threshold;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_THROTTLE_CONFIG_H_
