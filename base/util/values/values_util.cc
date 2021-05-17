// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/values/values_util.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"

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

base::Optional<int64_t> ValueToInt64(const base::Value* value) {
  return value ? ValueToInt64(*value) : base::nullopt;
}

base::Optional<int64_t> ValueToInt64(const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;

  int64_t integer;
  if (!base::StringToInt64(value.GetString(), &integer))
    return base::nullopt;

  return integer;
}

base::Value TimeDeltaToValue(base::TimeDelta time_delta) {
  return Int64ToValue(time_delta.InMicroseconds());
}

base::Optional<base::TimeDelta> ValueToTimeDelta(const base::Value* value) {
  return value ? ValueToTimeDelta(*value) : base::nullopt;
}

base::Optional<base::TimeDelta> ValueToTimeDelta(const base::Value& value) {
  base::Optional<int64_t> integer = ValueToInt64(value);
  if (!integer)
    return base::nullopt;
  return base::TimeDelta::FromMicroseconds(*integer);
}

base::Value TimeToValue(base::Time time) {
  return TimeDeltaToValue(time.ToDeltaSinceWindowsEpoch());
}

base::Optional<base::Time> ValueToTime(const base::Value* value) {
  return value ? ValueToTime(*value) : base::nullopt;
}

base::Optional<base::Time> ValueToTime(const base::Value& value) {
  base::Optional<base::TimeDelta> time_delta = ValueToTimeDelta(value);
  if (!time_delta)
    return base::nullopt;
  return base::Time::FromDeltaSinceWindowsEpoch(*time_delta);
}

base::Value FilePathToValue(base::FilePath file_path) {
  return base::Value(file_path.AsUTF8Unsafe());
}

base::Optional<base::FilePath> ValueToFilePath(const base::Value* value) {
  return value ? ValueToFilePath(*value) : base::nullopt;
}

base::Optional<base::FilePath> ValueToFilePath(const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;
  return base::FilePath::FromUTF8Unsafe(value.GetString());
}

base::Value UnguessableTokenToValue(base::UnguessableToken token) {
  UnguessableTokenRepresentation repr;
  repr.field.high = token.GetHighForSerialization();
  repr.field.low = token.GetLowForSerialization();
  return base::Value(base::HexEncode(repr.buffer, sizeof(repr.buffer)));
}

base::Optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value* value) {
  return value ? ValueToUnguessableToken(*value) : base::nullopt;
}

base::Optional<base::UnguessableToken> ValueToUnguessableToken(
    const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;
  UnguessableTokenRepresentation repr;
  if (!base::HexStringToSpan(value.GetString(), repr.buffer))
    return base::nullopt;
  return base::UnguessableToken::Deserialize(repr.field.high, repr.field.low);
}

}  // namespace util
