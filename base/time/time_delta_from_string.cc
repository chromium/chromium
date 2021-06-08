// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time_delta_from_string.h"

#include <limits>
#include <utility>

#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace base {

namespace {

// Strips the |expected| prefix from the start of the given string, returning
// |true| if the strip operation succeeded or false otherwise.
//
// Example:
//
//   StringPiece input("abc");
//   EXPECT_TRUE(ConsumePrefix(input, "a"));
//   EXPECT_EQ(input, "bc");
//
// Adapted from absl::ConsumePrefix():
// https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/strings/strip.h?l=45&rcl=2c22e9135f107a4319582ae52e2e3e6b201b6b7c
bool ConsumePrefix(StringPiece& str, StringPiece expected) {
  if (!StartsWith(str, expected))
    return false;
  str.remove_prefix(expected.size());
  return true;
}

// Utility struct used by ConsumeDurationNumber() to parse decimal numbers.
// A ParsedDecimal represents the number `int_part` + `frac_part`/`frac_scale`,
// where:
//  (i)  0 <= `frac_part` < `frac_scale` (implies `frac_part`/`frac_scale` < 1)
//  (ii) `frac_scale` is 10^[number of digits after the decimal point]
//
// Example:
//  -42 => {.int_part = -42, .frac_part = 0, .frac_scale = 1}
//  1.23 => {.int_part = 1, .frac_part = 23, .frac_scale = 100}
struct ParsedDecimal {
  int64_t int_part = 0;
  int64_t frac_part = 0;
  int64_t frac_scale = 1;
};

// A helper for FromString() that tries to parse a leading number from the given
// StringPiece. |number_string| is modified to start from the first unconsumed
// char.
//
// Adapted from absl:
// https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/time/duration.cc?l=807&rcl=2c22e9135f107a4319582ae52e2e3e6b201b6b7c
constexpr absl::optional<ParsedDecimal> ConsumeDurationNumber(
    StringPiece& number_string) {
  ParsedDecimal res;
  StringPiece::const_iterator orig_start = number_string.begin();
  // Parse contiguous digits.
  for (; !number_string.empty(); number_string.remove_prefix(1)) {
    const int d = number_string.front() - '0';
    if (d < 0 || d >= 10)
      break;

    if (res.int_part > std::numeric_limits<int64_t>::max() / 10)
      return absl::nullopt;
    res.int_part *= 10;
    if (res.int_part > std::numeric_limits<int64_t>::max() - d)
      return absl::nullopt;
    res.int_part += d;
  }
  const bool int_part_empty = number_string.begin() == orig_start;
  if (number_string.empty() || number_string.front() != '.')
    return int_part_empty ? absl::nullopt : absl::make_optional(res);

  number_string.remove_prefix(1);  // consume '.'
  // Parse contiguous digits.
  for (; !number_string.empty(); number_string.remove_prefix(1)) {
    const int d = number_string.front() - '0';
    if (d < 0 || d >= 10)
      break;
    DCHECK_LT(res.frac_part, res.frac_scale);
    if (res.frac_scale <= std::numeric_limits<int64_t>::max() / 10) {
      // |frac_part| will not overflow because it is always < |frac_scale|.
      res.frac_part *= 10;
      res.frac_part += d;
      res.frac_scale *= 10;
    }
  }

  return int_part_empty && res.frac_scale == 1 ? absl::nullopt
                                               : absl::make_optional(res);
}

// A helper for FromString() that tries to parse a leading unit designator
// (e.g., ns, us, ms, s, m, h) from the given StringPiece. |unit_string| is
// modified to start from the first unconsumed char.
//
// Adapted from absl:
// https://cs.chromium.org/chromium/src/third_party/abseil-cpp/absl/time/duration.cc?l=841&rcl=2c22e9135f107a4319582ae52e2e3e6b201b6b7c
absl::optional<TimeDelta> ConsumeDurationUnit(StringPiece& unit_string) {
  for (const auto& str_delta : {
           std::make_pair("ns", TimeDelta::FromNanoseconds(1)),
           std::make_pair("us", TimeDelta::FromMicroseconds(1)),
           // Note: "ms" MUST be checked before "m" to ensure that milliseconds
           // are not parsed as minutes.
           std::make_pair("ms", TimeDelta::FromMilliseconds(1)),
           std::make_pair("s", TimeDelta::FromSeconds(1)),
           std::make_pair("m", TimeDelta::FromMinutes(1)),
           std::make_pair("h", TimeDelta::FromHours(1)),
       }) {
    if (ConsumePrefix(unit_string, str_delta.first))
      return str_delta.second;
  }

  return absl::nullopt;
}

}  // namespace

absl::optional<TimeDelta> TimeDeltaFromString(StringPiece duration_string) {
  int sign = 1;
  if (ConsumePrefix(duration_string, "-"))
    sign = -1;
  else
    ConsumePrefix(duration_string, "+");
  if (duration_string.empty())
    return absl::nullopt;

  // Handle special-case values that don't require units.
  if (duration_string == "0")
    return TimeDelta();
  if (duration_string == "inf")
    return sign == 1 ? TimeDelta::Max() : TimeDelta::Min();

  TimeDelta delta;
  while (!duration_string.empty()) {
    absl::optional<ParsedDecimal> number_opt =
        ConsumeDurationNumber(duration_string);
    if (!number_opt.has_value())
      return absl::nullopt;
    absl::optional<TimeDelta> unit_opt = ConsumeDurationUnit(duration_string);
    if (!unit_opt.has_value())
      return absl::nullopt;

    ParsedDecimal number = number_opt.value();
    TimeDelta unit = unit_opt.value();
    if (number.int_part != 0)
      delta += sign * number.int_part * unit;
    if (number.frac_part != 0)
      delta +=
          (static_cast<double>(sign) * number.frac_part / number.frac_scale) *
          unit;
  }
  return delta;
}

}  // namespace base
