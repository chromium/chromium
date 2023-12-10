// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"

namespace ash {
namespace usage_time_limit {

class TimeLimitOverride {
 public:
  // Available override actions.
  enum class Action {
    kLock,   // Enforces user being blocked from accessing the session.
    kUnlock  // Enforces removing blockade from user session.
  };

  // Dictionary key for overrides used in serialization. Should match Time
  // Limits policy specification.
  static constexpr char kOverridesDictKey[] = "overrides";

  // Returns string that represents given |action| in serialized form. Should
  // match Time Limits policy.
  static std::string ActionToString(Action action);

  // Factory method. Creates TimeLimitOverride from a |dict|. Returns nullopt if
  // |dict| could not be parsed.
  static std::optional<TimeLimitOverride> FromDictionary(
      const base::Value::Dict* dict);

  // Factory method. Creates TimeLimitOverride from the most recent override in
  // the list of overrides passed in |list|. Returns nullopt if |list| could not
  // be parsed.
  static std::optional<TimeLimitOverride> MostRecentFromList(
      const base::Value::List* list);

  TimeLimitOverride(Action action,
                    base::Time created_at,
                    std::optional<base::TimeDelta> duration);

  TimeLimitOverride(const TimeLimitOverride&) = delete;
  TimeLimitOverride& operator=(const TimeLimitOverride&) = delete;

  ~TimeLimitOverride();
  TimeLimitOverride(TimeLimitOverride&&);
  TimeLimitOverride& operator=(TimeLimitOverride&&);
  bool operator==(const TimeLimitOverride&) const;
  bool operator!=(const TimeLimitOverride& rhs) const {
    return !(*this == rhs);
  }

  // Returns override duration if specified.
  Action action() const { return action_; }

  // Returns override creation time.
  base::Time created_at() const { return created_at_; }

  // Returns override duration if specified.
  std::optional<base::TimeDelta> duration() const { return duration_; }

  // Convenience method to quickly check if is this is a locking override.
  bool IsLock() const;

  // Serializes TimeLimitOverride to a dictionary.
  base::Value::Dict ToDictionary() const;

 private:
  Action action_;
  base::Time created_at_;
  std::optional<base::TimeDelta> duration_;
};

}  // namespace usage_time_limit
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_
