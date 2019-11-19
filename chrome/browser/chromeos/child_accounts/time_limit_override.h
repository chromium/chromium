// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {
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
  static base::Optional<TimeLimitOverride> FromDictionary(
      const base::Value* dict);

  // Factory method. Creates TimeLimitOverride from the most recent override in
  // the list of overrides passed in |list|. Returns nullopt if |list| could not
  // be parsed.
  static base::Optional<TimeLimitOverride> MostRecentFromList(
      const base::Value* list);

  TimeLimitOverride(Action action,
                    base::Time created_at,
                    base::Optional<base::TimeDelta> duration);
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
  base::Optional<base::TimeDelta> duration() const { return duration_; }

  // Convenience method to quickly check if is this is a locking override.
  bool IsLock() const;

  // Serializes TimeLimitOverride to a dictionary.
  base::Value ToDictionary() const;

 private:
  Action action_;
  base::Time created_at_;
  base::Optional<base::TimeDelta> duration_;

  DISALLOW_COPY_AND_ASSIGN(TimeLimitOverride);
};

}  // namespace usage_time_limit
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_OVERRIDE_H_
