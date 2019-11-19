// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_types.h"

namespace notifications {

Impression::Impression() = default;

Impression::Impression(SchedulerClientType type,
                       const std::string& guid,
                       const base::Time& create_time)
    : create_time(create_time), guid(guid), type(type) {}

Impression::Impression(const Impression& other) = default;

Impression::~Impression() = default;

bool Impression::operator==(const Impression& other) const {
  return create_time == other.create_time && feedback == other.feedback &&
         impression == other.impression && integrated == other.integrated &&
         guid == other.guid && type == other.type &&
         impression_mapping == other.impression_mapping &&
         custom_data == other.custom_data;
}

SuppressionInfo::SuppressionInfo(const base::Time& last_trigger,
                                 const base::TimeDelta& duration)
    : last_trigger_time(last_trigger), duration(duration), recover_goal(1) {}

SuppressionInfo::SuppressionInfo(const SuppressionInfo& other) = default;

bool SuppressionInfo::operator==(const SuppressionInfo& other) const {
  return last_trigger_time == other.last_trigger_time &&
         duration == other.duration && recover_goal == other.recover_goal;
}

base::Time SuppressionInfo::ReleaseTime() const {
  return last_trigger_time + duration;
}

ClientState::ClientState()
    : type(SchedulerClientType::kUnknown), current_max_daily_show(0) {}

ClientState::ClientState(const ClientState& other) = default;

ClientState::~ClientState() = default;

bool ClientState::operator==(const ClientState& other) const {
  if (impressions.size() != other.impressions.size())
    return false;

  for (size_t i = 0; i < impressions.size(); ++i) {
    if (!(impressions[i] == other.impressions[i]))
      return false;
  }

  return type == other.type &&
         current_max_daily_show == other.current_max_daily_show &&
         suppression_info == other.suppression_info;
}

}  // namespace notifications
