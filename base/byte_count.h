// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BYTE_COUNT_H_
#define BASE_BYTE_COUNT_H_

#include <concepts>
#include <cstdint>
#include <iosfwd>

#include "base/base_export.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"

namespace base {

// DEPRECATED: use ByteSize for unsigned values and ByteSizeDelta for signed.
//
// Represents an integral number of bytes. Supports arithmetic operations and
// conversions to/from KiB, MiB, GiB, TiB, PiB, and EiB. Any operation that
// overflows will result in a crash and thus this should only be used for
// trusted inputs.
//
// Sample usage:
//
//   // Share unit conversion code.
//   constexpr ByteCount kBufferSize = MiB(1);
//   std::vector<char> buffer(kBufferSize.InBytesUnsigned());
//
//   // Enforce that correct units are used across APIs at compile time.
//   ByteCount quota = GetQuota();
//   SetMetadataSize(base::KiB(10));
//   SetDatabaseSize(quota - base::KiB(10));
//
// KiB(), MiB() and GiB() can take float parameters. This will return the
// nearest integral number of bytes, rounding towards zero.
class BASE_EXPORT ByteCount {
 public:
  constexpr ByteCount() = default;

  constexpr explicit ByteCount(int64_t bytes) : bytes_(bytes) {}

  ~ByteCount() = default;

  ByteCount(const ByteCount&) = default;
  ByteCount& operator=(const ByteCount&) = default;

  static constexpr ByteCount FromUnsigned(uint64_t bytes) {
    return ByteCount(checked_cast<int64_t>(bytes));
  }

  static constexpr ByteCount FromChecked(
      const CheckedNumeric<int64_t>& checked_bytes) {
    return ByteCount(checked_bytes.ValueOrDie());
  }

  constexpr bool is_positive() const { return bytes_ > 0; }
  constexpr bool is_zero() const { return bytes_ == 0; }
  constexpr bool is_negative() const { return bytes_ < 0; }

  // A value corresponding to the "maximum" number of bytes possible. Useful as
  // a constant to mean "unlimited".
  static constexpr ByteCount Max();

  // Conversion to integral values.
  constexpr int64_t InBytes() const { return bytes_; }
  constexpr int64_t InKiB() const { return bytes_ / 1024; }
  constexpr int64_t InMiB() const { return bytes_ / 1024 / 1024; }
  constexpr int64_t InGiB() const { return bytes_ / 1024 / 1024 / 1024; }
  constexpr int64_t InTiB() const { return bytes_ / 1024 / 1024 / 1024 / 1024; }
  constexpr int64_t InPiB() const {
    return bytes_ / 1024 / 1024 / 1024 / 1024 / 1024;
  }
  constexpr int64_t InEiB() const {
    return bytes_ / 1024 / 1024 / 1024 / 1024 / 1024 / 1024;
  }

  // Conversion to floating point values.
  constexpr double InBytesF() const { return bytes_; }
  constexpr double InKiBF() const { return bytes_ / 1024.0; }
  constexpr double InMiBF() const { return bytes_ / 1024.0 / 1024.0; }
  constexpr double InGiBF() const { return bytes_ / 1024.0 / 1024.0 / 1024.0; }
  constexpr double InTiBF() const {
    return bytes_ / 1024.0 / 1024.0 / 1024.0 / 1024.0;
  }
  constexpr double InPiBF() const {
    return bytes_ / 1024.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0;
  }
  constexpr double InEiBF() const {
    return bytes_ / 1024.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0;
  }

  // Conversion to an unsigned amount of bytes. Only use when it is guaranteed
  // that the value is positive. Fails if the value is negative.
  constexpr uint64_t InBytesUnsigned() const {
    return checked_cast<uint64_t>(bytes_);
  }

  // Math operations.

  constexpr ByteCount operator+() const { return *this; }
  constexpr ByteCount operator-() const { return ByteCount(-bytes_); }

  constexpr ByteCount& operator+=(const ByteCount& other) {
    *this =
        ByteCount::FromChecked(CheckedNumeric<int64_t>(bytes_) + other.bytes_);
    return *this;
  }

  friend constexpr ByteCount operator+(ByteCount left, const ByteCount& right) {
    left += right;
    return left;
  }

  constexpr ByteCount& operator-=(const ByteCount& other) {
    *this =
        ByteCount::FromChecked(CheckedNumeric<int64_t>(bytes_) - other.bytes_);
    return *this;
  }

  friend constexpr ByteCount operator-(ByteCount left, const ByteCount& right) {
    left -= right;
    return left;
  }

  template <typename T>
  constexpr ByteCount& operator*=(const T& value) {
    *this = ByteCount::FromChecked(CheckedNumeric<int64_t>(bytes_) * value);
    return *this;
  }

  template <typename T>
  friend constexpr ByteCount operator*(ByteCount left, const T& right) {
    left *= right;
    return left;
  }

  template <typename T>
  friend constexpr ByteCount operator*(const T& left, ByteCount right) {
    right *= left;
    return right;
  }

  template <typename T>
  constexpr ByteCount& operator/=(const T& value) {
    *this = ByteCount::FromChecked(CheckedNumeric<int64_t>(bytes_) / value);
    return *this;
  }

  template <typename T>
  friend constexpr ByteCount operator/(ByteCount left, const T& right) {
    left /= right;
    return left;
  }

  constexpr friend bool operator==(const ByteCount& a,
                                   const ByteCount& b) = default;
  constexpr friend auto operator<=>(const ByteCount& a,
                                    const ByteCount& b) = default;

 private:
  int64_t bytes_ = 0;
};

// DEPRECATED: use KiBU, etc, for unsigned values and KiBS, etc, for signed.
//
// TODO(crbug.com/448661443): After all uses are migrated to explicit signed/
// unsigned, delete these and rename KiBU to KiB.
//
// Templated functions to construct from various types. Note that integers must
// be converted to CheckedNumeric<int64_t> BEFORE multiplying to detect
// overflows, while floats must be converted AFTER multiplying to avoid
// premature truncation.

template <typename T>
  requires std::integral<T>
constexpr ByteCount KiB(T kib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(kib) * 1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount KiB(T kib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(kib * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteCount MiB(T mib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(mib) * 1024 * 1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount MiB(T mib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(mib * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteCount GiB(T gib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(gib) * 1024 * 1024 *
                                1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount GiB(T gib) {
  return ByteCount::FromChecked(
      CheckedNumeric<int64_t>(gib * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteCount TiB(T tib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(tib) * 1024 * 1024 *
                                1024 * 1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount TiB(T gib) {
  return ByteCount::FromChecked(
      CheckedNumeric<int64_t>(gib * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteCount PiB(T pib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(pib) * 1024 * 1024 *
                                1024 * 1024 * 1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount PiB(T pib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(
      pib * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteCount EiB(T eib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(eib) * 1024 * 1024 *
                                1024 * 1024 * 1024 * 1024);
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteCount EiB(T eib) {
  return ByteCount::FromChecked(CheckedNumeric<int64_t>(
      eib * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

BASE_EXPORT std::ostream& operator<<(std::ostream& os, ByteCount byte_count);

// static
constexpr ByteCount ByteCount::Max() {
  return ByteCount(std::numeric_limits<int64_t>::max());
}

}  // namespace base

#endif  // BASE_BYTE_COUNT_H_
