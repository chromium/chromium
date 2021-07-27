// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/values_util.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Warning: The Values involved could be stored on persistent storage like files
// on disks. Therefore, changes in implementation could lead to data corruption
// and must be done with caution.

namespace base {

namespace {

// Helper to serialize/deserialize an UnguessableToken.
//
// It assumes a little-endian CPU, which is arguably a bug.
union UnguessableTokenRepresentation {
  struct Field {
    uint64_t high;
    uint64_t low;
  } field;

  uint8_t buffer[sizeof(Field)];
};

}  // namespace

Value Int64ToValue(int64_t integer) {
  return Value(NumberToString(integer));
}

absl::optional<int64_t> ValueToInt64(const Value* value) {
  return value ? ValueToInt64(*value) : absl::nullopt;
}

absl::optional<int64_t> ValueToInt64(const Value& value) {
  if (!value.is_string())
    return absl::nullopt;

  int64_t integer;
  if (!StringToInt64(value.GetString(), &integer))
    return absl::nullopt;

  return integer;
}

Value TimeDeltaToValue(TimeDelta time_delta) {
  return Int64ToValue(time_delta.InMicroseconds());
}

absl::optional<TimeDelta> ValueToTimeDelta(const Value* value) {
  return value ? ValueToTimeDelta(*value) : absl::nullopt;
}

absl::optional<TimeDelta> ValueToTimeDelta(const Value& value) {
  absl::optional<int64_t> integer = ValueToInt64(value);
  if (!integer)
    return absl::nullopt;
  return TimeDelta::FromMicroseconds(*integer);
}

Value TimeToValue(Time time) {
  return TimeDeltaToValue(time.ToDeltaSinceWindowsEpoch());
}

absl::optional<Time> ValueToTime(const Value* value) {
  return value ? ValueToTime(*value) : absl::nullopt;
}

absl::optional<Time> ValueToTime(const Value& value) {
  absl::optional<TimeDelta> time_delta = ValueToTimeDelta(value);
  if (!time_delta)
    return absl::nullopt;
  return Time::FromDeltaSinceWindowsEpoch(*time_delta);
}

Value FilePathToValue(FilePath file_path) {
  return Value(file_path.AsUTF8Unsafe());
}

absl::optional<FilePath> ValueToFilePath(const Value* value) {
  return value ? ValueToFilePath(*value) : absl::nullopt;
}

absl::optional<FilePath> ValueToFilePath(const Value& value) {
  if (!value.is_string())
    return absl::nullopt;
  return FilePath::FromUTF8Unsafe(value.GetString());
}

Value UnguessableTokenToValue(UnguessableToken token) {
  UnguessableTokenRepresentation repr;
  repr.field.high = token.GetHighForSerialization();
  repr.field.low = token.GetLowForSerialization();
  return Value(HexEncode(repr.buffer, sizeof(repr.buffer)));
}

absl::optional<UnguessableToken> ValueToUnguessableToken(const Value* value) {
  return value ? ValueToUnguessableToken(*value) : absl::nullopt;
}

absl::optional<UnguessableToken> ValueToUnguessableToken(const Value& value) {
  if (!value.is_string())
    return absl::nullopt;
  UnguessableTokenRepresentation repr;
  if (!HexStringToSpan(value.GetString(), repr.buffer))
    return absl::nullopt;
  return UnguessableToken::Deserialize(repr.field.high, repr.field.low);
}

}  // namespace base
