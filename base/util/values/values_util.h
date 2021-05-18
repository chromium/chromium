// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_VALUES_VALUES_UTIL_H_
#define BASE_UTIL_VALUES_VALUES_UTIL_H_

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
class UnguessableToken;
}  // namespace base

namespace util {

// Simple helper functions for converting between base::Value and other types.
// The base::Value representation is stable, suitable for persistent storage
// e.g. as JSON on disk.
//
// It is valid to pass nullptr to the ValueToEtc functions. They will just
// return absl::nullopt.

// Converts between an int64_t and a string-flavored base::Value (a human
// readable string of that number).
base::Value Int64ToValue(int64_t integer);
absl::optional<int64_t> ValueToInt64(const base::Value* value);
absl::optional<int64_t> ValueToInt64(const base::Value& value);

// Converts between a base::TimeDelta (an int64_t number of microseconds) and a
// string-flavored base::Value (a human readable string of that number).
base::Value TimeDeltaToValue(base::TimeDelta time_delta);
absl::optional<base::TimeDelta> ValueToTimeDelta(const base::Value* value);
absl::optional<base::TimeDelta> ValueToTimeDelta(const base::Value& value);

// Converts between a base::Time (an int64_t number of microseconds since the
// Windows epoch) and a string-flavored base::Value (a human readable string of
// that number).
base::Value TimeToValue(base::Time time);
absl::optional<base::Time> ValueToTime(const base::Value* value);
absl::optional<base::Time> ValueToTime(const base::Value& value);

// Converts between a base::FilePath (a std::string or std::u16string) and a
// string-flavored base::Value (the UTF-8 representation).
base::Value FilePathToValue(base::FilePath file_path);
absl::optional<base::FilePath> ValueToFilePath(const base::Value* value);
absl::optional<base::FilePath> ValueToFilePath(const base::Value& value);

// Converts between a base::UnguessableToken (128 bits) and a string-flavored
// base::Value (32 hexadecimal digits).
base::Value UnguessableTokenToValue(base::UnguessableToken token);
absl::optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value* value);
absl::optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value& value);

}  // namespace util

#endif  // BASE_UTIL_VALUES_VALUES_UTIL_H_
