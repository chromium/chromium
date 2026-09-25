// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/values_util.h"

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"

// Warning: The Values involved could be stored on persistent storage like files
// on disks. Therefore, changes in implementation could lead to data corruption
// and must be done with caution.

namespace base {

Value Int64ToValue(int64_t integer) {
  return Value(NumberToString(integer));
}

std::optional<int64_t> ValueToInt64(const Value* value) {
  return value ? ValueToInt64(*value) : std::nullopt;
}

std::optional<int64_t> ValueToInt64(const Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return std::nullopt;
  }

  int64_t integer;
  if (!StringToInt64(*str, &integer)) {
    return std::nullopt;
  }

  return integer;
}

Value TimeDeltaToValue(TimeDelta time_delta) {
  return Int64ToValue(time_delta.InMicroseconds());
}

std::optional<TimeDelta> ValueToTimeDelta(const Value* value) {
  return value ? ValueToTimeDelta(*value) : std::nullopt;
}

std::optional<TimeDelta> ValueToTimeDelta(const Value& value) {
  std::optional<int64_t> integer = ValueToInt64(value);
  if (!integer) {
    return std::nullopt;
  }
  return Microseconds(*integer);
}

Value TimeToValue(Time time) {
  return TimeDeltaToValue(time.ToDeltaSinceWindowsEpoch());
}

std::optional<Time> ValueToTime(const Value* value) {
  return value ? ValueToTime(*value) : std::nullopt;
}

std::optional<Time> ValueToTime(const Value& value) {
  std::optional<TimeDelta> time_delta = ValueToTimeDelta(value);
  if (!time_delta) {
    return std::nullopt;
  }
  return Time::FromDeltaSinceWindowsEpoch(*time_delta);
}

Value FilePathToValue(const FilePath& file_path) {
  return Value(file_path.AsUTF8Unsafe());
}

std::optional<FilePath> ValueToFilePath(const Value* value) {
  return value ? ValueToFilePath(*value) : std::nullopt;
}

std::optional<FilePath> ValueToFilePath(const Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return std::nullopt;
  }
  return FilePath::FromUTF8Unsafe(*str);
}

Value UnguessableTokenToValue(UnguessableToken token) {
  return Value(HexEncode(token.AsBytes()));
}

std::optional<UnguessableToken> ValueToUnguessableToken(const Value* value) {
  return value ? ValueToUnguessableToken(*value) : std::nullopt;
}

std::optional<UnguessableToken> ValueToUnguessableToken(const Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return std::nullopt;
  }
  std::array<uint64_t, 2> words;
  if (!HexStringToSpan(*str, as_writable_byte_span(words))) {
    return std::nullopt;
  }
  return UnguessableToken::Deserialize(words[0], words[1]);
}

}  // namespace base
