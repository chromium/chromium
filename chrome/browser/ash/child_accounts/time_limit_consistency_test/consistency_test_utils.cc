// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limit_consistency_test/consistency_test_utils.h"

#include "base/check.h"

#include "chrome/browser/ash/child_accounts/time_limit_consistency_test/goldens/consistency_golden.pb.h"

namespace ash {
namespace time_limit_consistency_utils {

void AddWindowLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    const TimeOfDay& starts_at,
    const TimeOfDay& ends_at,
    std::optional<int64_t> last_updated) {
  time_limit_consistency::ConsistencyGoldenWindowLimitEntry* window =
      golden_input->add_window_limits();
  window->mutable_starts_at()->set_hour(starts_at.hour);
  window->mutable_starts_at()->set_minute(starts_at.minute);
  window->mutable_ends_at()->set_hour(ends_at.hour);
  window->mutable_ends_at()->set_minute(ends_at.minute);
  window->set_effective_day(effective_day);

  if (last_updated)
    window->set_last_updated_millis(last_updated.value());
}

void AddUsageLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int usage_quota_mins,
    std::optional<int64_t> last_updated) {
  time_limit_consistency::ConsistencyGoldenUsageLimitEntry* usage_limit =
      golden_input->add_usage_limits();
  usage_limit->set_usage_quota_mins(usage_quota_mins);
  usage_limit->set_effective_day(effective_day);

  if (last_updated)
    usage_limit->set_last_updated_millis(last_updated.value());
}

void AddOverrideToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenOverrideAction action,
    int64_t created_at) {
  DCHECK(action != time_limit_consistency::UNLOCK_UNTIL_LOCK_DEADLINE);

  time_limit_consistency::ConsistencyGoldenOverride* override_entry =
      golden_input->add_overrides();
  override_entry->set_action(action);
  override_entry->set_created_at_millis(created_at);
}

void AddTimedOverrideToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    int64_t duration_millis,
    int64_t created_at) {
  time_limit_consistency::ConsistencyGoldenOverride* override_entry =
      golden_input->add_overrides();
  override_entry->set_action(
      time_limit_consistency::UNLOCK_UNTIL_LOCK_DEADLINE);
  override_entry->set_duration_millis(duration_millis);
  override_entry->set_created_at_millis(created_at);
}

}  // namespace time_limit_consistency_utils
}  // namespace ash
