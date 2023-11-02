// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/throttle_config.h"

namespace notifications {

ThrottleConfig::ThrottleConfig() = default;

ThrottleConfig::ThrottleConfig(const ThrottleConfig& other) = default;

bool ThrottleConfig::operator==(const ThrottleConfig& other) const {
  return suppression_duration == other.suppression_duration &&
         negative_action_count_threshold ==
             other.negative_action_count_threshold;
}

ThrottleConfig::~ThrottleConfig() = default;

}  // namespace notifications
