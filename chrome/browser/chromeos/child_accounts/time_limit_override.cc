// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_override.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {
namespace usage_time_limit {

namespace {

constexpr char kOverrideAction[] = "action";
constexpr char kOverrideActionCreatedAt[] = "created_at_millis";
constexpr char kOverrideActionDurationMins[] = "duration_mins";
constexpr char kOverrideActionLock[] = "LOCK";
constexpr char kOverrideActionUnlock[] = "UNLOCK";
constexpr char kOverrideActionSpecificData[] = "action_specific_data";
constexpr char kOverrideActionDuration[] = "duration_mins";

// Returns string containing |timestamp| int64_t value in milliseconds. This is
// how timestamp is sent in a policy.
std::string PolicyTimestamp(base::Time timestamp) {
  return std::to_string((timestamp - base::Time::UnixEpoch()).InMilliseconds());
}

}  // namespace

// static
constexpr char TimeLimitOverride::kOverridesDictKey[];

// static
std::string TimeLimitOverride::ActionToString(Action action) {
  switch (action) {
    case Action::kLock:
      return kOverrideActionLock;
    case Action::kUnlock:
      return kOverrideActionUnlock;
  }
}

// static
base::Optional<TimeLimitOverride> TimeLimitOverride::FromDictionary(
    const base::Value* dict) {
  if (!dict || !dict->is_dict()) {
    DLOG(ERROR) << "Override entry is not a dictionary";
    return base::nullopt;
  }

  const std::string* action_string = dict->FindStringKey(kOverrideAction);
  if (!action_string || action_string->empty()) {
    DLOG(ERROR) << "Invalid override action.";
    return base::nullopt;
  }

  const std::string* creation_time_string =
      dict->FindStringKey(kOverrideActionCreatedAt);
  int64_t creation_time_millis;
  if (!creation_time_string || creation_time_string->empty() ||
      !base::StringToInt64(*creation_time_string, &creation_time_millis)) {
    DLOG(ERROR) << "Invalid override creation time.";
    return base::nullopt;
  }

  Action action =
      *action_string == kOverrideActionLock ? Action::kLock : Action::kUnlock;

  base::Time creation_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(creation_time_millis);

  const base::Value* duration_value = dict->FindPath(
      {kOverrideActionSpecificData, kOverrideActionDurationMins});
  base::Optional<base::TimeDelta> duration =
      duration_value ? base::TimeDelta::FromMinutes(duration_value->GetInt())
                     : base::Optional<base::TimeDelta>();

  return TimeLimitOverride(action, creation_time, duration);
}

// static
base::Optional<TimeLimitOverride> TimeLimitOverride::MostRecentFromList(
    const base::Value* list) {
  if (!list || !list->is_list()) {
    DLOG(ERROR) << "Override entries should be a list.";
    return base::nullopt;
  }

  // The most recent override created.
  base::Optional<TimeLimitOverride> last_override;
  for (const base::Value& override_value : list->GetList()) {
    base::Optional<TimeLimitOverride> current_override =
        FromDictionary(&override_value);
    if (!current_override.has_value()) {
      DLOG(ERROR) << "Invalid override entry";
      continue;
    }

    if (!last_override.has_value() ||
        (current_override->created_at() > last_override->created_at())) {
      last_override = std::move(current_override);
    }
  }
  return last_override;
}

TimeLimitOverride::TimeLimitOverride(Action action,
                                     base::Time created_at,
                                     base::Optional<base::TimeDelta> duration)
    : action_(action), created_at_(created_at), duration_(duration) {}

TimeLimitOverride::~TimeLimitOverride() = default;

TimeLimitOverride::TimeLimitOverride(TimeLimitOverride&&) = default;

TimeLimitOverride& TimeLimitOverride::operator=(TimeLimitOverride&&) = default;

bool TimeLimitOverride::operator==(const TimeLimitOverride& rhs) const {
  return action_ == rhs.action() && created_at_ == rhs.created_at() &&
         duration_ == rhs.duration();
}

bool TimeLimitOverride::IsLock() const {
  return action_ == Action::kLock;
}

base::Value TimeLimitOverride::ToDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kOverrideAction, base::Value(ActionToString(action_)));
  dict.SetKey(kOverrideActionCreatedAt,
              base::Value(PolicyTimestamp(created_at_)));
  if (duration_.has_value()) {
    base::Value duration_dict(base::Value::Type::DICTIONARY);
    duration_dict.SetKey(kOverrideActionDuration,
                         base::Value(duration_->InMinutes()));
    dict.SetKey(kOverrideActionSpecificData, std::move(duration_dict));
  }
  return dict;
}

}  // namespace usage_time_limit
}  // namespace chromeos
