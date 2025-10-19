// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BYTE_SIZE_H_
#define BASE_BYTE_SIZE_H_

#include <concepts>
#include <cstdint>
#include <iosfwd>

#include "base/base_export.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"

namespace base {

// ByteSize (unsigned, 64-bit) and ByteSizeDelta (signed, 64-bit) each represent
// an integral number of bytes. They support arithmetic operations and
// conversions to/from KiB, MiB, GiB, TiB, PiB, and EiB.
//
// The range of ByteSize is [0...max(int64_t)] so that it's a strict subset of
// ByteSizeDelta, to simplify conversion rules. Any operation that overflows
// (including converting a negative ByteSizeDelta to ByteSize) will result in a
// crash and thus this should only be used for trusted inputs.
//
// Sample usage:
//
//   // Do not reinvent conversion between units.
//   constexpr ByteSize kBufferSize = MiBU(1);
//   std::vector<char> buffer(kBufferSize.InBytes());
//
//   // Enforce that correct units are used across APIs at compile time.
//   ByteSize quota = GetQuota();
//   SetMetadataSize(KiBU(10));
//   ByteSizeDelta remaining_quota = quota - KiBU(10);
//   SetDatabaseSize(remaining_quota.AsByteSize());
//
// KiBU()/KiBS(), MiBU()/MiBS(), etc. can take float parameters. This will
// return the nearest integral number of bytes, rounding towards zero.

namespace internal {

// Shared code between ByteSize and ByteSizeDelta.
//
// This wraps a number of bytes stored as int64_t, which covers the full range
// of both ByteSize ([0...max(int64_t)]) and ByteSizeDelta.
class ByteSizeBase {
 public:
  ~ByteSizeBase() = default;

  // Comparators and hashers.
  constexpr friend bool operator==(ByteSizeBase a, ByteSizeBase b) = default;
  constexpr friend auto operator<=>(ByteSizeBase a, ByteSizeBase b) = default;

  template <typename H>
  friend H AbslHashValue(H h, ByteSizeBase b) {
    return H::combine(std::move(h), b.bytes_);
  }

 protected:
  // Can only be constructed by subclasses.
  constexpr ByteSizeBase() = default;

  template <typename T>
  constexpr explicit ByteSizeBase(T bytes)
      : bytes_(checked_cast<int64_t>(bytes)) {}

  // Conversion helper functions.
  template <typename ReturnType>
  constexpr ReturnType InBytesImpl() const {
    return checked_cast<ReturnType>(bytes_);
  }
  template <typename ReturnType>
  constexpr ReturnType InKiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024;
  }
  template <typename ReturnType>
  constexpr ReturnType InMiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024 / 1024;
  }
  template <typename ReturnType>
  constexpr ReturnType InGiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024 / 1024 / 1024;
  }
  template <typename ReturnType>
  constexpr ReturnType InTiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024 / 1024 / 1024 / 1024;
  }
  template <typename ReturnType>
  constexpr ReturnType InPiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024 / 1024 / 1024 / 1024 / 1024;
  }
  template <typename ReturnType>
  constexpr ReturnType InEiBImpl() const {
    return InBytesImpl<ReturnType>() / 1024 / 1024 / 1024 / 1024 / 1024 / 1024;
  }

  // Returns the wrapped value as a CheckedNumeric. Always use int64_t for
  // checked math so that intermediate steps can handle the full range of
  // ByteSizeDelta. The ByteSize constructor will CHECK if a final ByteSize
  // result is negative.
  constexpr CheckedNumeric<int64_t> AsChecked() const { return bytes_; }

  // Helpers to implement math operations, storing the result in ResultType.
  // These all modify a CheckedNumeric in-place because
  // `operator+(CheckedNumeric<int64_t>, uint64_t)` will return a
  // CheckedNumeric<uint64_t>, failing on any negative value.

  template <typename ResultType>
    requires std::derived_from<ResultType, ByteSizeBase>
  constexpr ResultType AddImpl(ByteSizeBase other) const {
    auto checked_bytes = AsChecked();
    checked_bytes += other.bytes_;
    return ResultType(checked_bytes);
  }

  template <typename ResultType>
    requires std::derived_from<ResultType, ByteSizeBase>
  constexpr ResultType SubImpl(ByteSizeBase other) const {
    auto checked_bytes = AsChecked();
    checked_bytes -= other.bytes_;
    return ResultType(checked_bytes);
  }

  template <typename ResultType, typename T>
    requires std::derived_from<ResultType, ByteSizeBase>
  constexpr ResultType MulImpl(T value) const {
    auto checked_bytes = AsChecked();
    checked_bytes *= value;
    return ResultType(checked_bytes);
  }

  template <typename ResultType, typename T>
    requires std::derived_from<ResultType, ByteSizeBase>
  constexpr ResultType DivImpl(T value) const {
    auto checked_bytes = AsChecked();
    checked_bytes /= value;
    return ResultType(checked_bytes);
  }

 private:
  // The wrapped number of bytes. Not stored as a CheckedNumeric because it only
  // needs to be checked on modification. It doesn't carry around errors. This
  // is private so that it can't be modified by subclasses without going through
  // the constructor, which range-checks it.
  int64_t bytes_ = 0;
};

}  // namespace internal

class ByteSizeDelta;

// A non-negative number of bytes, in the range [0...max(int64_t)].
class BASE_EXPORT ByteSize : public internal::ByteSizeBase {
 public:
  constexpr ByteSize() = default;

  // Constructs a ByteSize from an unsigned integer. CHECK's that the value is
  // in range.
  template <typename T>
    requires std::unsigned_integral<T>
  constexpr explicit ByteSize(T bytes)
      // The inherited constructor ensures `bytes` is also in range for int64_t.
      : ByteSizeBase(checked_cast<uint64_t>(bytes)) {}

  // Constructs a ByteSize from a compile-time constant with signed type, for
  // convenience. Constants that are out of range will fail to compile.
  //
  // To construct a ByteSize from a signed integer at runtime, cast explicitly
  // or use FromByteSizeDelta().
  template <typename T>
    requires std::signed_integral<T>
  consteval explicit ByteSize(T bytes)
      // The inherited constructor ensures `bytes` is also in range for int64_t.
      : ByteSizeBase(checked_cast<uint64_t>(bytes)) {}

  // Converts ByteSize to and from a signed ByteSizeDelta. Converting from a
  // delta CHECK's that it's in range (ie. non-negative). Converting to a delta
  // always succeeds.
  static constexpr ByteSize FromByteSizeDelta(ByteSizeDelta delta);
  constexpr ByteSizeDelta AsByteSizeDelta() const;

  // Returns a value corresponding to the "maximum" number of bytes possible.
  // Useful as a constant to mean "unlimited".
  static constexpr ByteSize Max() {
    return ByteSize(std::numeric_limits<int64_t>::max());
  }

  constexpr bool is_zero() const { return InBytes() == 0; }

  constexpr bool is_max() const { return *this == Max(); }

  // Conversion to integral values.
  constexpr uint64_t InBytes() const { return InBytesImpl<uint64_t>(); }
  constexpr uint64_t InKiB() const { return InKiBImpl<uint64_t>(); }
  constexpr uint64_t InMiB() const { return InMiBImpl<uint64_t>(); }
  constexpr uint64_t InGiB() const { return InGiBImpl<uint64_t>(); }
  constexpr uint64_t InTiB() const { return InTiBImpl<uint64_t>(); }
  constexpr uint64_t InPiB() const { return InPiBImpl<uint64_t>(); }
  constexpr uint64_t InEiB() const { return InEiBImpl<uint64_t>(); }

  // Conversion to floating point values.
  constexpr double InBytesF() const { return InBytesImpl<double>(); }
  constexpr double InKiBF() const { return InKiBImpl<double>(); }
  constexpr double InMiBF() const { return InMiBImpl<double>(); }
  constexpr double InGiBF() const { return InGiBImpl<double>(); }
  constexpr double InTiBF() const { return InTiBImpl<double>(); }
  constexpr double InPiBF() const { return InPiBImpl<double>(); }
  constexpr double InEiBF() const { return InEiBImpl<double>(); }

  // Math operators. Addition and subtraction deliberately support only ByteSize
  // and ByteSizeDelta, to make sure all values are constructed with explicit
  // units.

  constexpr ByteSize& operator+=(ByteSize other) {
    return *this = AddImpl<ByteSize>(other);
  }
  constexpr ByteSize& operator+=(ByteSizeDelta delta);

  constexpr ByteSize& operator-=(ByteSize other) {
    return *this = SubImpl<ByteSize>(other);
  }
  constexpr ByteSize& operator-=(ByteSizeDelta delta);

  template <typename T>
  constexpr ByteSize& operator*=(T value) {
    return *this = MulImpl<ByteSize>(value);
  }

  template <typename T>
  constexpr ByteSize& operator/=(T value) {
    return *this = DivImpl<ByteSize>(value);
  }

  // Returns the sum of two ByteSizes.
  friend constexpr ByteSize operator+(ByteSize left, ByteSize right) {
    return left.AddImpl<ByteSize>(right);
  }

  // Returns the delta between two ByteSizes.
  friend constexpr ByteSizeDelta operator-(ByteSize left, ByteSize right);

  // Modifies a ByteSize by a delta, and returns the result.
  friend constexpr ByteSize operator+(ByteSize left, ByteSizeDelta right);
  friend constexpr ByteSize operator+(ByteSizeDelta left, ByteSize right);
  friend constexpr ByteSize operator-(ByteSize left, ByteSizeDelta right);

  // Scales a ByteSize by a numeric value, and returns the result.
  template <typename T>
  friend constexpr ByteSize operator*(ByteSize left, T right) {
    return left.MulImpl<ByteSize>(right);
  }
  template <typename T>
  friend constexpr ByteSize operator*(T left, ByteSize right) {
    return right * left;
  }
  template <typename T>
  friend constexpr ByteSize operator/(ByteSize left, T right) {
    return left.DivImpl<ByteSize>(right);
  }

 private:
  // Allow ByteSizeBase::AddImpl, etc. to construct from the math result.
  friend class ByteSizeBase;

  constexpr explicit ByteSize(CheckedNumeric<int64_t> checked_bytes)
      : ByteSizeBase(checked_cast<uint64_t>(checked_bytes.ValueOrDie())) {}
};

// A signed number of bytes, in the range [min(int64_t)...max(int64_t)].
class BASE_EXPORT ByteSizeDelta : public internal::ByteSizeBase {
 public:
  constexpr ByteSizeDelta() = default;

  // Constructs a ByteSizeDelta from a signed integer.
  template <typename T>
    requires std::signed_integral<T>
  constexpr explicit ByteSizeDelta(T bytes) : ByteSizeBase(bytes) {}

  // Constructs a ByteSizeDelta from a compile-time constant with unsigned type,
  // for convenience. Constants that are out of range will fail to compile.
  //
  // To construct a ByteSizeDelta from an unsigned integer at runtime, cast
  // explicitly or use FromByteSize().
  template <typename T>
    requires std::unsigned_integral<T>
  consteval explicit ByteSizeDelta(T bytes)
      // The inherited constructor ensures `bytes` is in range for int64_t.
      : ByteSizeBase(bytes) {}

  // Converts ByteSizeDelta to and from an unsigned ByteSize. Converting from a
  // delta CHECK's that it's in range (ie. non-negative). Converting to a delta
  // always succeeds.
  static constexpr ByteSizeDelta FromByteSize(ByteSize size) {
    return size.AsByteSizeDelta();
  }
  constexpr ByteSize AsByteSize() const {
    return ByteSize(checked_cast<uint64_t>(InBytes()));
  }

  // Returns a value corresponding to the "maximum" (positive) number of bytes
  // possible. Useful as a constant to mean "unlimited" in the positive
  // direction.
  static constexpr ByteSizeDelta Max() {
    return ByteSizeDelta(std::numeric_limits<int64_t>::max());
  }

  // Returns a value corresponding to the "minimum" (or maximum negative) number
  // of bytes possible. Useful as a constant to mean "unlimited" in the negative
  // direction.
  static constexpr ByteSizeDelta Min() {
    return ByteSizeDelta(std::numeric_limits<int64_t>::min());
  }

  constexpr bool is_positive() const { return InBytes() > 0; }
  constexpr bool is_zero() const { return InBytes() == 0; }
  constexpr bool is_negative() const { return InBytes() < 0; }

  constexpr bool is_max() const { return *this == Max(); }
  constexpr bool is_min() const { return *this == Min(); }

  // Conversion to integral values.
  constexpr int64_t InBytes() const { return InBytesImpl<int64_t>(); }
  constexpr int64_t InKiB() const { return InKiBImpl<int64_t>(); }
  constexpr int64_t InMiB() const { return InMiBImpl<int64_t>(); }
  constexpr int64_t InGiB() const { return InGiBImpl<int64_t>(); }
  constexpr int64_t InTiB() const { return InTiBImpl<int64_t>(); }
  constexpr int64_t InPiB() const { return InPiBImpl<int64_t>(); }
  constexpr int64_t InEiB() const { return InEiBImpl<int64_t>(); }

  // Conversion to floating point values.
  constexpr double InBytesF() const { return InBytesImpl<double>(); }
  constexpr double InKiBF() const { return InKiBImpl<double>(); }
  constexpr double InMiBF() const { return InMiBImpl<double>(); }
  constexpr double InGiBF() const { return InGiBImpl<double>(); }
  constexpr double InTiBF() const { return InTiBImpl<double>(); }
  constexpr double InPiBF() const { return InPiBImpl<double>(); }
  constexpr double InEiBF() const { return InEiBImpl<double>(); }

  // Returns the absolute value, as a ByteSizeDelta. CHECK's that the absolute
  // value is in range (ie. not Min(), since two's complement minimums have no
  // corresponding positive value in range.)
  constexpr ByteSizeDelta Abs() const {
    return ByteSizeDelta(AsChecked().Abs());
  }

  // Returns the absolute value, as a ByteSize. CHECK's that the absolute value
  // is in range for a ByteSize.
  constexpr ByteSize Magnitude() const { return Abs().AsByteSize(); }

  // Math operators. Addition and subtraction deliberately support only
  // ByteSizeDelta, to make sure all values are constructed with explicit units.

  constexpr ByteSizeDelta operator+() const { return *this; }
  constexpr ByteSizeDelta operator-() const {
    return ByteSizeDelta(-AsChecked());
  }

  constexpr ByteSizeDelta& operator+=(ByteSizeDelta other) {
    return *this = AddImpl<ByteSizeDelta>(other);
  }
  constexpr ByteSizeDelta& operator-=(ByteSizeDelta other) {
    return *this = SubImpl<ByteSizeDelta>(other);
  }

  template <typename T>
  constexpr ByteSizeDelta& operator*=(T value) {
    return *this = MulImpl<ByteSizeDelta>(value);
  }

  template <typename T>
  constexpr ByteSizeDelta& operator/=(T value) {
    return *this = DivImpl<ByteSizeDelta>(value);
  }

  friend constexpr ByteSizeDelta operator+(ByteSizeDelta left,
                                           ByteSizeDelta right) {
    return left.AddImpl<ByteSizeDelta>(right);
  }

  friend constexpr ByteSizeDelta operator-(ByteSizeDelta left,
                                           ByteSizeDelta right) {
    return left.SubImpl<ByteSizeDelta>(right);
  }

  template <typename T>
  friend constexpr ByteSizeDelta operator*(ByteSizeDelta left, T right) {
    return left.MulImpl<ByteSizeDelta>(right);
  }
  template <typename T>
  friend constexpr ByteSizeDelta operator*(T left, ByteSizeDelta right) {
    return right * left;
  }

  template <typename T>
  friend constexpr ByteSizeDelta operator/(ByteSizeDelta left, T right) {
    return left.DivImpl<ByteSizeDelta>(right);
  }

 private:
  // Allow ByteSizeBase::AddImpl, etc. to construct from the math result.
  friend class ByteSizeBase;

  constexpr explicit ByteSizeDelta(CheckedNumeric<int64_t> checked_bytes)
      : ByteSizeBase(checked_bytes.ValueOrDie()) {}
};

// Templated functions to construct from various types. Note that integers must
// be converted to CheckedNumeric BEFORE multiplying to detect overflows, while
// floats must be converted AFTER multiplying to avoid premature truncation.
//
// TODO(crbug.com/448661443): After all uses of KiB, etc, are migrated to
// explicit signed/ unsigned, rename KiBU to KiB.

template <typename T>
  requires std::integral<T>
constexpr ByteSize KiBU(T kib) {
  return ByteSize(kib) * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize KiBU(T kib) {
  return ByteSize(checked_cast<uint64_t>(kib * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta KiBS(T kib) {
  return ByteSizeDelta(kib) * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta KiBS(T kib) {
  return ByteSizeDelta(checked_cast<int64_t>(kib * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSize MiBU(T mib) {
  return ByteSize(mib) * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize MiBU(T mib) {
  return ByteSize(checked_cast<uint64_t>(mib * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta MiBS(T mib) {
  return ByteSizeDelta(mib) * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta MiBS(T mib) {
  return ByteSizeDelta(checked_cast<int64_t>(mib * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSize GiBU(T gib) {
  return ByteSize(gib) * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize GiBU(T gib) {
  return ByteSize(checked_cast<uint64_t>(gib * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta GiBS(T gib) {
  return ByteSizeDelta(gib) * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta GiBS(T gib) {
  return ByteSizeDelta(checked_cast<int64_t>(gib * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSize TiBU(T tib) {
  return ByteSize(tib) * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize TiBU(T tib) {
  return ByteSize(
      checked_cast<uint64_t>(tib * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta TiBS(T tib) {
  return ByteSizeDelta(tib) * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta TiBS(T tib) {
  return ByteSizeDelta(
      checked_cast<int64_t>(tib * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSize PiBU(T pib) {
  return ByteSize(pib) * 1024 * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize PiBU(T pib) {
  return ByteSize(
      checked_cast<uint64_t>(pib * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta PiBS(T pib) {
  return ByteSizeDelta(pib) * 1024 * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta PiBS(T pib) {
  return ByteSizeDelta(
      checked_cast<int64_t>(pib * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSize EiBU(T eib) {
  return ByteSize(eib) * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSize EiBU(T eib) {
  return ByteSize(checked_cast<uint64_t>(eib * 1024.0 * 1024.0 * 1024.0 *
                                         1024.0 * 1024.0 * 1024.0));
}

template <typename T>
  requires std::integral<T>
constexpr ByteSizeDelta EiBS(T eib) {
  return ByteSizeDelta(eib) * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
}

template <typename T>
  requires std::floating_point<T>
constexpr ByteSizeDelta EiBS(T eib) {
  return ByteSizeDelta(checked_cast<int64_t>(eib * 1024.0 * 1024.0 * 1024.0 *
                                             1024.0 * 1024.0 * 1024.0));
}

// Stream operators for logging and testing.

BASE_EXPORT std::ostream& operator<<(std::ostream& os, ByteSize size);
BASE_EXPORT std::ostream& operator<<(std::ostream& os, ByteSizeDelta delta);

// Implementation.

// static
constexpr ByteSize ByteSize::FromByteSizeDelta(ByteSizeDelta delta) {
  return delta.AsByteSize();
}

constexpr ByteSizeDelta ByteSize::AsByteSizeDelta() const {
  return ByteSizeDelta(checked_cast<int64_t>(InBytes()));
}

constexpr ByteSize& ByteSize::operator+=(ByteSizeDelta delta) {
  return *this = AddImpl<ByteSize>(delta);
}

constexpr ByteSize& ByteSize::operator-=(ByteSizeDelta delta) {
  return *this = SubImpl<ByteSize>(delta);
}

constexpr ByteSizeDelta operator-(ByteSize left, ByteSize right) {
  return left.SubImpl<ByteSizeDelta>(right);
}

constexpr ByteSize operator+(ByteSize left, ByteSizeDelta right) {
  return left.AddImpl<ByteSize>(right);
}

constexpr ByteSize operator+(ByteSizeDelta left, ByteSize right) {
  return right + left;
}

constexpr ByteSize operator-(ByteSize left, ByteSizeDelta right) {
  return left.SubImpl<ByteSize>(right);
}

}  // namespace base

#endif  // BASE_BYTE_SIZE_H_
