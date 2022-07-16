// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_VALUES_UTIL_H_
#define BASE_JSON_VALUES_UTIL_H_

#include "base/base_export.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
class UnguessableToken;

// Simple helper functions for converting between Value and other types.
// The Value representation is stable, suitable for persistent storage
// e.g. as JSON on disk.
//
// It is valid to pass nullptr to the ValueToEtc functions. They will just
// return absl::nullopt.

// Converts between an int64_t and a string-flavored Value (a human
// readable string of that number).
BASE_EXPORT Value Int64ToValue(int64_t integer);
BASE_EXPORT absl::optional<int64_t> ValueToInt64(const Value* value);
BASE_EXPORT absl::optional<int64_t> ValueToInt64(const Value& value);

// Converts between a TimeDelta (an int64_t number of microseconds) and a
// string-flavored Value (a human readable string of that number).
BASE_EXPORT Value TimeDeltaToValue(TimeDelta time_delta);
BASE_EXPORT absl::optional<TimeDelta> ValueToTimeDelta(const Value* value);
BASE_EXPORT absl::optional<TimeDelta> ValueToTimeDelta(const Value& value);

// Converts between a Time (an int64_t number of microseconds since the
// Windows epoch) and a string-flavored Value (a human readable string of
// that number).
BASE_EXPORT Value TimeToValue(Time time);
BASE_EXPORT absl::optional<Time> ValueToTime(const Value* value);
BASE_EXPORT absl::optional<Time> ValueToTime(const Value& value);

// Converts between a FilePath (a std::string or std::u16string) and a
// string-flavored Value (the UTF-8 representation).
BASE_EXPORT Value FilePathToValue(FilePath file_path);
BASE_EXPORT absl::optional<FilePath> ValueToFilePath(const Value* value);
BASE_EXPORT absl::optional<FilePath> ValueToFilePath(const Value& value);

// Converts between a UnguessableToken (128 bits) and a string-flavored
// Value (32 hexadecimal digits).
BASE_EXPORT Value UnguessableTokenToValue(UnguessableToken token);
BASE_EXPORT absl::optional<UnguessableToken> ValueToUnguessableToken(
    const Value* value);
BASE_EXPORT absl::optional<UnguessableToken> ValueToUnguessableToken(
    const Value& value);

}  // namespace base

#endif  // BASE_JSON_VALUES_UTIL_H_
