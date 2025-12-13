// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_size.h"

#include <ostream>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace base {

namespace {

constexpr uint64_t kOneKiB = KiBU(1).InBytes();
constexpr uint64_t kOneMiB = MiBU(1).InBytes();
constexpr uint64_t kOneGiB = GiBU(1).InBytes();
constexpr uint64_t kOneTiB = TiBU(1).InBytes();
constexpr uint64_t kOnePiB = PiBU(1).InBytes();
constexpr uint64_t kOneEiB = EiBU(1).InBytes();

void AppendMagnitude(std::string* dest,
                     ByteSize magnitude,
                     const std::string& sign_prefix = "") {
  uint64_t bytes = magnitude.InBytes();

  StrAppend(dest, {sign_prefix});

  // If it's an exact number of [EPTGMK]kB then stream that, unless it's a
  // quantity measurable by the next magnitude prefix (e.g. if the value is in
  // the pebibyte range but it happens to be divisible by 1024 it shouldn't be
  // logged in KiB).
  if (bytes % kOneEiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InEiB()), "EiB"});
  } else if (bytes % kOnePiB == 0 && bytes / kOneEiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InPiB()), "PiB"});
  } else if (bytes % kOneTiB == 0 && bytes / kOnePiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InTiB()), "TiB"});
  } else if (bytes % kOneGiB == 0 && bytes / kOneTiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InGiB()), "GiB"});
  } else if (bytes % kOneMiB == 0 && bytes / kOneGiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InMiB()), "MiB"});
  } else if (bytes % kOneKiB == 0 && bytes / kOneMiB == 0) {
    StrAppend(dest, {NumberToString(magnitude.InKiB()), "KiB"});
  } else {
    // If not, then stream the exact byte count plus (if larger than 1KiB) an
    // estimate for scale.
    StrAppend(dest, {NumberToString(bytes), "B"});
    if (bytes > kOneKiB) {
      StrAppend(dest, {" (", sign_prefix});
      if (bytes > kOneEiB) {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InEiBF(), 3), "EiB"});
      } else if (bytes > kOnePiB) {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InPiBF(), 3), "PiB"});
      } else if (bytes > kOneTiB) {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InTiBF(), 3), "TiB"});
      } else if (bytes > kOneGiB) {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InGiBF(), 3), "GiB"});
      } else if (bytes > kOneMiB) {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InMiBF(), 3), "MiB"});
      } else {
        StrAppend(
            dest,
            {NumberToStringWithFixedPrecision(magnitude.InKiBF(), 3), "KiB"});
      }
      StrAppend(dest, {")"});
    }
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& os, ByteSize size) {
  // If it's exactly 0 then stream and return.
  if (size.is_zero()) {
    return os << "0B";
  }

  // Reserve enough space (e.g. "1152920504606846976B (1023.999PiB)" which is
  // a full 64-bit value with four digits before the decimal).
  std::string result;
  result.reserve(34);

  AppendMagnitude(&result, size);
  return os << result;
}

std::ostream& operator<<(std::ostream& os, ByteSizeDelta delta) {
  // If it's exactly 0 then stream and return.
  if (delta.is_zero()) {
    return os << "0B";
  }

  // If it's exactly INT64_MIN then stream and return. Later in this function,
  // negative values are handled by processing their absolute value, but
  // INT64_MIN, like all two's complement minimums, has no corresponding
  // positive value within range.
  if (delta.is_min()) {
    return os << "-8EiB";
  }

  // Reserve enough space (e.g. "-1152920504606846976B (-1023.999PiB)" which is
  // a full 64-bit negative value with four digits before the decimal).
  std::string result;
  result.reserve(36);

  // Format that magnitude, with negative signs prepended if necessary.
  AppendMagnitude(&result, delta.Magnitude(), delta.is_negative() ? "-" : "");
  return os << result;
}

}  // namespace base
