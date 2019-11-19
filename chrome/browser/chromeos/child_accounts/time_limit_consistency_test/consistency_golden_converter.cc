// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"

namespace chromeos {

namespace utils = time_limit_test_utils;

namespace time_limit_consistency {
namespace {

// The default resets_at value is 6AM.
constexpr base::TimeDelta kDefaultResetsAt = base::TimeDelta::FromHours(6);

// Converts a PolicyType object from the time limit processor to a
// ConsistencyGoldenPolicy used by the goldens.
ConsistencyGoldenPolicy ConvertProcessorPolicyToGoldenPolicy(
    usage_time_limit::PolicyType processor_policy) {
  switch (processor_policy) {
    case usage_time_limit::PolicyType::kOverride:
      return OVERRIDE;
    case usage_time_limit::PolicyType::kFixedLimit:
      return FIXED_LIMIT;
    case usage_time_limit::PolicyType::kUsageLimit:
      return USAGE_LIMIT;
    case usage_time_limit::PolicyType::kNoPolicy:
      return NO_ACTIVE_POLICY;
  }

  NOTREACHED();
  return UNSPECIFIED_POLICY;
}

// Converts the representation of a day of week used by the goldens to the one
// used by the time limit processor.
const char* ConvertGoldenDayToProcessorDay(ConsistencyGoldenEffectiveDay day) {
  switch (day) {
    case MONDAY:
      return utils::kMonday;
    case TUESDAY:
      return utils::kTuesday;
    case WEDNESDAY:
      return utils::kWednesday;
    case THURSDAY:
      return utils::kThursday;
    case FRIDAY:
      return utils::kFriday;
    case SATURDAY:
      return utils::kSaturday;
    case SUNDAY:
      return utils::kSunday;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

base::Value ConvertGoldenInputToProcessorInput(
    const ConsistencyGoldenInput& input) {
  // Random date representing the last time the policies were updated,
  // used whenever the last_updated field is not specified in the input proto.
  base::Time default_last_updated =
      utils::TimeFromString("1 Jan 2018 10:00 GMT+0300");
  base::TimeDelta resets_at =
      input.has_usage_limit_resets_at()
          ? utils::CreateTime(input.usage_limit_resets_at().hour(),
                              input.usage_limit_resets_at().minute())
          : kDefaultResetsAt;

  base::Value policy = utils::CreateTimeLimitPolicy(resets_at);

  /* Begin Window Limits data */

  for (const ConsistencyGoldenWindowLimitEntry& window_limit :
       input.window_limits()) {
    utils::AddTimeWindowLimit(
        &policy, ConvertGoldenDayToProcessorDay(window_limit.effective_day()),
        utils::CreateTime(window_limit.starts_at().hour(),
                          window_limit.starts_at().minute()),
        utils::CreateTime(window_limit.ends_at().hour(),
                          window_limit.ends_at().minute()),
        window_limit.has_last_updated_millis()
            ? base::Time::FromJavaTime(window_limit.last_updated_millis())
            : default_last_updated);
  }

  /* End Window Limits data */
  /* Begin Usage Limits data */

  for (const ConsistencyGoldenUsageLimitEntry& usage_limit :
       input.usage_limits()) {
    utils::AddTimeUsageLimit(
        &policy, ConvertGoldenDayToProcessorDay(usage_limit.effective_day()),
        base::TimeDelta::FromMinutes(usage_limit.usage_quota_mins()),
        usage_limit.has_last_updated_millis()
            ? base::Time::FromJavaTime(usage_limit.last_updated_millis())
            : default_last_updated);
  }

  /* End Usage Limits data */
  /* Begin Overrides data */

  for (const ConsistencyGoldenOverride& override_entry : input.overrides()) {
    if (override_entry.action() == UNLOCK_UNTIL_LOCK_DEADLINE) {
      utils::AddOverrideWithDuration(
          &policy, usage_time_limit::TimeLimitOverride::Action::kUnlock,
          base::Time::FromJavaTime(override_entry.created_at_millis()),
          base::TimeDelta::FromMilliseconds(override_entry.duration_millis()));
    } else {
      utils::AddOverride(
          &policy,
          override_entry.action() == LOCK
              ? usage_time_limit::TimeLimitOverride::Action::kLock
              : usage_time_limit::TimeLimitOverride::Action::kUnlock,
          base::Time::FromJavaTime(override_entry.created_at_millis()));
    }
  }

  /* End Overrides data */

  return policy;
}

ConsistencyGoldenOutput ConvertProcessorOutputToGoldenOutput(
    const usage_time_limit::State& state) {
  ConsistencyGoldenOutput golden_output;

  golden_output.set_is_locked(state.is_locked);
  golden_output.set_active_policy(
      ConvertProcessorPolicyToGoldenPolicy(state.active_policy));
  golden_output.set_next_active_policy(
      ConvertProcessorPolicyToGoldenPolicy(state.next_state_active_policy));

  if (state.is_time_usage_limit_enabled &&
      golden_output.active_policy() != OVERRIDE) {
    golden_output.set_remaining_quota_millis(
        state.remaining_usage.InMilliseconds());
  }

  if (state.is_locked) {
    golden_output.set_next_unlocking_time_millis(
        state.next_unlock_time.ToJavaTime());
  }

  return golden_output;
}

base::Optional<usage_time_limit::State>
GenerateUnlockUsageLimitOverrideStateFromInput(
    const ConsistencyGoldenInput& input) {
  const ConsistencyGoldenOverride* usage_limit_override = nullptr;
  for (const ConsistencyGoldenOverride& override_entry : input.overrides()) {
    if (override_entry.action() == UNLOCK_USAGE_LIMIT &&
        (!usage_limit_override ||
         override_entry.created_at_millis() >
             usage_limit_override->created_at_millis())) {
      usage_limit_override = &override_entry;
    }
  }

  if (!usage_limit_override)
    return base::nullopt;

  usage_time_limit::State previous_state;
  previous_state.is_locked = true;
  previous_state.active_policy = usage_time_limit::PolicyType::kUsageLimit;
  previous_state.is_time_usage_limit_enabled = true;
  previous_state.remaining_usage = base::TimeDelta::FromMinutes(0);

  // Usage limit started one minute before the override was created.
  previous_state.time_usage_limit_started =
      base::Time::FromJavaTime(usage_limit_override->created_at_millis()) -
      base::TimeDelta::FromMinutes(1);

  return previous_state;
}

}  // namespace time_limit_consistency
}  // namespace chromeos
