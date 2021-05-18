// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/values/values_util.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Warning: The Values involved could be stored on persistent storage like files
// on disks. Therefore, changes in implementation could lead to data corruption
// and must be done with caution.

namespace util {

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

base::Value Int64ToValue(int64_t integer) {
  return base::Value(base::NumberToString(integer));
}

absl::optional<int64_t> ValueToInt64(const base::Value* value) {
  return value ? ValueToInt64(*value) : absl::nullopt;
}

absl::optional<int64_t> ValueToInt64(const base::Value& value) {
  if (!value.is_string())
    return absl::nullopt;

  int64_t integer;
  if (!base::StringToInt64(value.GetString(), &integer))
    return absl::nullopt;

  return integer;
}

base::Value TimeDeltaToValue(base::TimeDelta time_delta) {
  return Int64ToValue(time_delta.InMicroseconds());
}

absl::optional<base::TimeDelta> ValueToTimeDelta(const base::Value* value) {
  return value ? ValueToTimeDelta(*value) : absl::nullopt;
}

absl::optional<base::TimeDelta> ValueToTimeDelta(const base::Value& value) {
  absl::optional<int64_t> integer = ValueToInt64(value);
  if (!integer)
    return absl::nullopt;
  return base::TimeDelta::FromMicroseconds(*integer);
}

base::Value TimeToValue(base::Time time) {
  return TimeDeltaToValue(time.ToDeltaSinceWindowsEpoch());
}

absl::optional<base::Time> ValueToTime(const base::Value* value) {
  return value ? ValueToTime(*value) : absl::nullopt;
}

absl::optional<base::Time> ValueToTime(const base::Value& value) {
  absl::optional<base::TimeDelta> time_delta = ValueToTimeDelta(value);
  if (!time_delta)
    return absl::nullopt;
  return base::Time::FromDeltaSinceWindowsEpoch(*time_delta);
}

base::Value FilePathToValue(base::FilePath file_path) {
  return base::Value(file_path.AsUTF8Unsafe());
}

absl::optional<base::FilePath> ValueToFilePath(const base::Value* value) {
  return value ? ValueToFilePath(*value) : absl::nullopt;
}

absl::optional<base::FilePath> ValueToFilePath(const base::Value& value) {
  if (!value.is_string())
    return absl::nullopt;
  return base::FilePath::FromUTF8Unsafe(value.GetString());
}

base::Value UnguessableTokenToValue(base::UnguessableToken token) {
  UnguessableTokenRepresentation repr;
  repr.field.high = token.GetHighForSerialization();
  repr.field.low = token.GetLowForSerialization();
  return base::Value(base::HexEncode(repr.buffer, sizeof(repr.buffer)));
}

absl::optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value* value) {
  return value ? ValueToUnguessableToken(*value) : absl::nullopt;
}

absl::optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value& value) {
  if (!value.is_string())
    return absl::nullopt;
  UnguessableTokenRepresentation repr;
  if (!base::HexStringToSpan(value.GetString(), repr.buffer))
    return absl::nullopt;
  return base::UnguessableToken::Deserialize(repr.field.high, repr.field.low);
}

}  // namespace util
