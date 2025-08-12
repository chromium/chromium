// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_count.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"

namespace base {

std::ostream& operator<<(std::ostream& os, ByteCount byte_count) {
  int64_t bytes = byte_count.InBytes();

  // If it's exactly 0 then stream and return.
  if (bytes == 0) {
    os << "0B";
    return os;
  }

  // If it's exactly INT64_MIN then stream and return. Later in this function,
  // negative values are handled by processing their absolute value, but
  // INT64_MIN, like all two's complement minimums, has no corresponding
  // positive value within range.
  if (bytes == std::numeric_limits<int64_t>::min()) {
    os << "-8EiB";
    return os;
  }

  // Reserve enough space (e.g. "-1152920504606846976B (-1023.999PiB)" which is
  // a full 64-bit negative value with four digits before the decimal).
  std::string result;
  result.reserve(36);

  // Separate out the sign, as it's easier to do magnitude tests on positive
  // values.
  bool is_negative = bytes < 0;
  if (is_negative) {
    bytes = -bytes;
    byte_count = -byte_count;
    result += "-";
  }

  // If it's an exact number of [EPTGMK]kB then stream that, unless it's a
  // quantity measurable by the next magnitude prefix (e.g. if the value is in
  // the pebibyte range but it happens to be divisible by 1024 it shouldn't be
  // logged in KiB).
  if (bytes % EiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InEiB());
    result += "EiB";
  } else if (bytes % PiB(1).InBytes() == 0 && bytes / EiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InPiB());
    result += "PiB";
  } else if (bytes % TiB(1).InBytes() == 0 && bytes / PiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InTiB());
    result += "TiB";
  } else if (bytes % GiB(1).InBytes() == 0 && bytes / TiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InGiB());
    result += "GiB";
  } else if (bytes % MiB(1).InBytes() == 0 && bytes / GiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InMiB());
    result += "MiB";
  } else if (bytes % KiB(1).InBytes() == 0 && bytes / MiB(1).InBytes() == 0) {
    result += NumberToString(byte_count.InKiB());
    result += "KiB";
  } else {
    // If not, then stream the exact byte count plus (if larger than 1KiB) an
    // estimate for scale.
    result += NumberToString(bytes);
    result += "B";
    if (bytes > KiB(1).InBytes()) {
      result += " (";
      if (is_negative) {
        result += "-";
      }
      if (bytes > EiB(1).InBytes()) {
        result += NumberToStringWithFixedPrecision(byte_count.InEiBF(), 3);
        result += "EiB";
      } else if (bytes > PiB(1).InBytes()) {
        result += NumberToStringWithFixedPrecision(byte_count.InPiBF(), 3);
        result += "PiB";
      } else if (bytes > TiB(1).InBytes()) {
        result += NumberToStringWithFixedPrecision(byte_count.InTiBF(), 3);
        result += "TiB";
      } else if (bytes > GiB(1).InBytes()) {
        result += NumberToStringWithFixedPrecision(byte_count.InGiBF(), 3);
        result += "GiB";
      } else if (bytes > MiB(1).InBytes()) {
        result += NumberToStringWithFixedPrecision(byte_count.InMiBF(), 3);
        result += "MiB";
      } else {
        result += NumberToStringWithFixedPrecision(byte_count.InKiBF(), 3);
        result += "KiB";
      }
      result += ")";
    }
  }

  os << result;
  return os;
}

}  // namespace base
