// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <type_traits>

#include "base/compiler_specific.h"
#include "build/build_config.h"

// WARNING: This block must come before the base/numerics headers are included.
// These tests deliberately cause arithmetic boundary errors. If the compiler is
// aggressive enough, it can const detect these errors, so we disable warnings.
#if BUILDFLAG(IS_WIN)
#pragma warning(disable : 4756)  // Arithmetic overflow.
#pragma warning(disable : 4293)  // Invalid shift.
#endif

// This may not need to come before the base/numerics headers, but let's keep
// it close to the MSVC equivalent.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winteger-overflow"
#endif

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/numerics/wrapping_math.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(COMPILER_MSVC) && defined(ARCH_CPU_32_BITS)
#include <mmintrin.h>
#endif

namespace base {
namespace internal {

using std::numeric_limits;

// This is a helper function for finding the maximum value in Src that can be
// wholy represented as the destination floating-point type.
template <typename Dst, typename Src>
Dst GetMaxConvertibleToFloat() {
  using DstLimits = numeric_limits<Dst>;
  using SrcLimits = numeric_limits<Src>;
  static_assert(SrcLimits::is_specialized, "Source must be numeric.");
  static_assert(DstLimits::is_specialized, "Destination must be numeric.");
  CHECK(DstLimits::is_iec559);

  if (SrcLimits::digits <= DstLimits::digits &&
      MaxExponent<Src>::value <= MaxExponent<Dst>::value)
    return SrcLimits::max();
  Src max = SrcLimits::max() / 2 + (SrcLimits::is_integer ? 1 : 0);
  while (max != static_cast<Src>(static_cast<Dst>(max))) {
    max /= 2;
  }
  return static_cast<Dst>(max);
}

// Test corner case promotions used
static_assert(IsIntegerArithmeticSafe<int32_t, int8_t, int8_t>::value, "");
static_assert(IsIntegerArithmeticSafe<int32_t, int16_t, int8_t>::value, "");
static_assert(IsIntegerArithmeticSafe<int32_t, int8_t, int16_t>::value, "");
static_assert(!IsIntegerArithmeticSafe<int32_t, int32_t, int8_t>::value, "");
static_assert(BigEnoughPromotion<int16_t, int8_t>::is_contained, "");
static_assert(BigEnoughPromotion<int32_t, uint32_t>::is_contained, "");
static_assert(BigEnoughPromotion<intmax_t, int8_t>::is_contained, "");
static_assert(!BigEnoughPromotion<uintmax_t, int8_t>::is_contained, "");
static_assert(
    std::is_same_v<BigEnoughPromotion<int16_t, int8_t>::type, int16_t>,
    "");
static_assert(
    std::is_same_v<BigEnoughPromotion<int32_t, uint32_t>::type, int64_t>,
    "");
static_assert(
    std::is_same_v<BigEnoughPromotion<intmax_t, int8_t>::type, intmax_t>,
    "");
static_assert(
    std::is_same_v<BigEnoughPromotion<uintmax_t, int8_t>::type, uintmax_t>,
    "");
static_assert(BigEnoughPromotion<int16_t, int8_t>::is_contained, "");
static_assert(BigEnoughPromotion<int32_t, uint32_t>::is_contained, "");
static_assert(BigEnoughPromotion<intmax_t, int8_t>::is_contained, "");
static_assert(!BigEnoughPromotion<uintmax_t, int8_t>::is_contained, "");
static_assert(
    std::is_same_v<FastIntegerArithmeticPromotion<int16_t, int8_t>::type,
                   int32_t>,
    "");
static_assert(
    std::is_same_v<FastIntegerArithmeticPromotion<int32_t, uint32_t>::type,
                   int64_t>,
    "");
static_assert(
    std::is_same_v<FastIntegerArithmeticPromotion<intmax_t, int8_t>::type,
                   intmax_t>,
    "");
static_assert(
    std::is_same_v<FastIntegerArithmeticPromotion<uintmax_t, int8_t>::type,
                   uintmax_t>,
    "");
static_assert(FastIntegerArithmeticPromotion<int16_t, int8_t>::is_contained,
              "");
static_assert(FastIntegerArithmeticPromotion<int32_t, uint32_t>::is_contained,
              "");
static_assert(!FastIntegerArithmeticPromotion<intmax_t, int8_t>::is_contained,
              "");
static_assert(!FastIntegerArithmeticPromotion<uintmax_t, int8_t>::is_contained,
              "");

// Test compile-time (constexpr) evaluation of checking and saturation.
constexpr int32_t kIntOne = 1;
static_assert(1 == checked_cast<uint8_t>(kIntOne), "");
static_assert(1 == saturated_cast<uint8_t>(kIntOne), "");
static_assert(2U == MakeClampedNum(kIntOne) + 1, "");
static_assert(2U == (MakeCheckedNum(kIntOne) + 1).ValueOrDie(), "");
static_assert(0U == MakeClampedNum(kIntOne) - 1, "");
static_assert(0U == (MakeCheckedNum(kIntOne) - 1).ValueOrDie(), "");
static_assert(-1 == -MakeClampedNum(kIntOne), "");
static_assert(-1 == (-MakeCheckedNum(kIntOne)).ValueOrDie(), "");
static_assert(1U == MakeClampedNum(kIntOne) * 1, "");
static_assert(1U == (MakeCheckedNum(kIntOne) * 1).ValueOrDie(), "");
static_assert(1U == MakeClampedNum(kIntOne) / 1, "");
static_assert(1U == (MakeCheckedNum(kIntOne) / 1).ValueOrDie(), "");
static_assert(1 == MakeClampedNum(-kIntOne).Abs(), "");
static_assert(1 == MakeCheckedNum(-kIntOne).Abs().ValueOrDie(), "");
static_assert(1U == MakeClampedNum(kIntOne) % 2, "");
static_assert(1U == (MakeCheckedNum(kIntOne) % 2).ValueOrDie(), "");
static_assert(0U == MakeClampedNum(kIntOne) >> 1U, "");
static_assert(0U == (MakeCheckedNum(kIntOne) >> 1U).ValueOrDie(), "");
static_assert(2U == MakeClampedNum(kIntOne) << 1U, "");
static_assert(2U == (MakeCheckedNum(kIntOne) << 1U).ValueOrDie(), "");
static_assert(1 == MakeClampedNum(kIntOne) & 1U, "");
static_assert(1 == (MakeCheckedNum(kIntOne) & 1U).ValueOrDie(), "");
static_assert(1 == MakeClampedNum(kIntOne) | 1U, "");
static_assert(1 == (MakeCheckedNum(kIntOne) | 1U).ValueOrDie(), "");
static_assert(0 == MakeClampedNum(kIntOne) ^ 1U, "");
static_assert(0 == (MakeCheckedNum(kIntOne) ^ 1U).ValueOrDie(), "");
constexpr float kFloatOne = 1.0;
static_assert(1 == int{checked_cast<int8_t>(kFloatOne)}, "");
static_assert(1 == int{saturated_cast<int8_t>(kFloatOne)}, "");
static_assert(2U == unsigned{MakeClampedNum(kFloatOne) + 1}, "");
static_assert(2U ==
                  (MakeCheckedNum(kFloatOne) + 1).Cast<unsigned>().ValueOrDie(),
              "");
static_assert(0U == unsigned{MakeClampedNum(kFloatOne) - 1}, "");
static_assert(0U ==
                  (MakeCheckedNum(kFloatOne) - 1).Cast<unsigned>().ValueOrDie(),
              "");
static_assert(-1 == int{-MakeClampedNum(kFloatOne)}, "");
static_assert(-1 == (-MakeCheckedNum(kFloatOne)).Cast<int>().ValueOrDie(), "");
static_assert(1U == unsigned{MakeClampedNum(kFloatOne) * 1}, "");
static_assert(1U ==
                  (MakeCheckedNum(kFloatOne) * 1).Cast<unsigned>().ValueOrDie(),
              "");
static_assert(1U == unsigned{MakeClampedNum(kFloatOne) / 1}, "");
static_assert(1U ==
                  (MakeCheckedNum(kFloatOne) / 1).Cast<unsigned>().ValueOrDie(),
              "");
static_assert(1 == int{MakeClampedNum(-kFloatOne).Abs()}, "");
static_assert(1 == MakeCheckedNum(-kFloatOne).Abs().Cast<int>().ValueOrDie(),
              "");

template <typename U>
U GetNumericValueForTest(const CheckedNumeric<U>& src) {
  return src.state_.value();
}

template <typename U>
U GetNumericValueForTest(const ClampedNumeric<U>& src) {
  return static_cast<U>(src);
}

template <typename U>
U GetNumericValueForTest(const U& src) {
  return src;
}

// Logs the ValueOrDie() failure instead of crashing.
struct LogOnFailure {
  template <typename T>
  static T HandleFailure() {
    LOG(WARNING) << "ValueOrDie() failed unexpectedly.";
    return T();
  }
};

template <typename T>
constexpr T GetValue(const T& src) {
  return src;
}

template <typename T, typename U>
constexpr T GetValueAsDest(const U& src) {
  return static_cast<T>(src);
}

template <typename T>
constexpr T GetValue(const CheckedNumeric<T>& src) {
  return src.template ValueOrDie<T, LogOnFailure>();
}

template <typename T, typename U>
constexpr T GetValueAsDest(const CheckedNumeric<U>& src) {
  return src.template ValueOrDie<T, LogOnFailure>();
}

template <typename T>
constexpr T GetValue(const ClampedNumeric<T>& src) {
  return static_cast<T>(src);
}

template <typename T, typename U>
constexpr T GetValueAsDest(const ClampedNumeric<U>& src) {
  return static_cast<T>(src);
}

// Helper macros to wrap displaying the conversion types and line numbers.
#define TEST_EXPECTED_VALIDITY(expected, actual)                           \
  EXPECT_EQ(expected, (actual).template Cast<Dst>().IsValid())             \
      << "Result test: Value " << GetNumericValueForTest(actual) << " as " \
      << dst << " on line " << line

#define TEST_EXPECTED_SUCCESS(actual) TEST_EXPECTED_VALIDITY(true, actual)
#define TEST_EXPECTED_FAILURE(actual) TEST_EXPECTED_VALIDITY(false, actual)

// We have to handle promotions, so infer the underlying type below from actual.
#define TEST_EXPECTED_VALUE(expected, actual)                               \
  EXPECT_EQ(GetValue(expected), GetValueAsDest<decltype(expected)>(actual)) \
      << "Result test: Value " << GetNumericValueForTest(actual) << " as "  \
      << dst << " on line " << line

// Test the simple pointer arithmetic overrides.
template <typename Dst>
void TestStrictPointerMath() {
  Dst dummy_value = 0;
  Dst* dummy_ptr = &dummy_value;
  static const Dst kDummyOffset = 2;  // Don't want to go too far.
  EXPECT_EQ(dummy_ptr + kDummyOffset,
            dummy_ptr + StrictNumeric<Dst>(kDummyOffset));
  EXPECT_EQ(dummy_ptr - kDummyOffset,
            dummy_ptr - StrictNumeric<Dst>(kDummyOffset));
  EXPECT_NE(dummy_ptr, dummy_ptr + StrictNumeric<Dst>(kDummyOffset));
  EXPECT_NE(dummy_ptr, dummy_ptr - StrictNumeric<Dst>(kDummyOffset));
  EXPECT_DEATH_IF_SUPPORTED(
      dummy_ptr + StrictNumeric<size_t>(std::numeric_limits<size_t>::max()),
      "");
}

// Signed integer arithmetic.
template <typename Dst>
static void TestSpecializedArithmetic(
    const char* dst,
    int line,
    std::enable_if_t<numeric_limits<Dst>::is_integer &&
                         numeric_limits<Dst>::is_signed,
                     int> = 0) {
  using DstLimits = SaturationDefaultLimits<Dst>;
  TEST_EXPECTED_FAILURE(-CheckedNumeric<Dst>(DstLimits::lowest()));
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(-1).Abs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeCheckedNum(-DstLimits::max()).Abs());

  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      -ClampedNumeric<Dst>(DstLimits::lowest()));
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(-1).Abs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeClampedNum(-DstLimits::max()).Abs());

  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) + -1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) +
                        DstLimits::lowest());

  TEST_EXPECTED_VALUE(DstLimits::max() - 1,
                      ClampedNumeric<Dst>(DstLimits::max()) + -1);
  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_VALUE(
      DstLimits::Underflow(),
      ClampedNumeric<Dst>(DstLimits::lowest()) + DstLimits::lowest());

  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) - 1);
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()) - -1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) -
                        DstLimits::lowest());
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) -
                        DstLimits::max());

  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) - 1);
  TEST_EXPECTED_VALUE(DstLimits::lowest() + 1,
                      ClampedNumeric<Dst>(DstLimits::lowest()) - -1);
  TEST_EXPECTED_VALUE(
      DstLimits::Overflow(),
      ClampedNumeric<Dst>(DstLimits::max()) - DstLimits::lowest());
  TEST_EXPECTED_VALUE(
      DstLimits::Underflow(),
      ClampedNumeric<Dst>(DstLimits::lowest()) - DstLimits::max());

  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) * 2);
  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) * 2);

  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) / -1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(-1) / 2);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) * -1);
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      CheckedNumeric<Dst>(DstLimits::lowest() + 1) * Dst(-1));
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      CheckedNumeric<Dst>(-1) * Dst(DstLimits::lowest() + 1));
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      CheckedNumeric<Dst>(DstLimits::lowest()) * Dst(1));
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      CheckedNumeric<Dst>(1) * Dst(DstLimits::lowest()));
  TEST_EXPECTED_VALUE(
      typename std::make_unsigned<Dst>::type(0) - DstLimits::lowest(),
      MakeCheckedNum(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeCheckedNum(DstLimits::max()).UnsignedAbs());
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(-1).UnsignedAbs());

  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) / -1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(-1) / 2);
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) * -1);
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      ClampedNumeric<Dst>(DstLimits::lowest() + 1) * Dst(-1));
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      ClampedNumeric<Dst>(-1) * Dst(DstLimits::lowest() + 1));
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) * Dst(1));
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      ClampedNumeric<Dst>(1) * Dst(DstLimits::lowest()));
  TEST_EXPECTED_VALUE(
      typename std::make_unsigned<Dst>::type(0) - DstLimits::lowest(),
      MakeClampedNum(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeClampedNum(DstLimits::max()).UnsignedAbs());
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(-1).UnsignedAbs());

  // Modulus is legal only for integers.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) % -1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) % -2);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % -1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) % -2);
  TEST_EXPECTED_VALUE(-1, CheckedNumeric<Dst>(-1) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(-1) % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(-1) % -1);
  TEST_EXPECTED_VALUE(-1, CheckedNumeric<Dst>(-1) % -2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) % -1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) % -2);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(DstLimits::max()) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::max()) % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::max()) % -1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(DstLimits::max()) % -2);
  // Test all the different modulus combinations.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 % CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
  CheckedNumeric<Dst> checked_dst = 1;
  TEST_EXPECTED_VALUE(0, checked_dst %= 1);
  // Test that div by 0 is avoided but returns invalid result.
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) % 0);
  // Test bit shifts.
  volatile Dst negative_one = -1;
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) << negative_one);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1)
                        << (IntegerBitsPlusSign<Dst>::value - 1));
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(0)
                        << IntegerBitsPlusSign<Dst>::value);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) << 1);
  TEST_EXPECTED_VALUE(
      static_cast<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 2),
      CheckedNumeric<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 2));
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0)
                             << (IntegerBitsPlusSign<Dst>::value - 1));
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) << 0);
  TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) << 1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) >>
                        IntegerBitsPlusSign<Dst>::value);
  TEST_EXPECTED_VALUE(
      0, CheckedNumeric<Dst>(1) >> (IntegerBitsPlusSign<Dst>::value - 1));
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) >> negative_one);

  // Modulus is legal only for integers.
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) % -1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) % -2);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % -1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) % -2);
  TEST_EXPECTED_VALUE(-1, ClampedNumeric<Dst>(-1) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(-1) % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(-1) % -1);
  TEST_EXPECTED_VALUE(-1, ClampedNumeric<Dst>(-1) % -2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) % -1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) % -2);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(DstLimits::max()) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::max()) % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::max()) % -1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(DstLimits::max()) % -2);
  // Test all the different modulus combinations.
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 % ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % 1);
  ClampedNumeric<Dst> clamped_dst = 1;
  TEST_EXPECTED_VALUE(0, clamped_dst %= 1);
  TEST_EXPECTED_VALUE(Dst(1), ClampedNumeric<Dst>(1) % 0);
  // Test bit shifts.
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(1)
                          << (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(Dst(0), ClampedNumeric<Dst>(0)
                                  << (IntegerBitsPlusSign<Dst>::value + 0U));
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::max()) << 1U);
  TEST_EXPECTED_VALUE(
      static_cast<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 2U),
      ClampedNumeric<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 2U));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0)
                             << (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) << 0U);
  TEST_EXPECTED_VALUE(2, ClampedNumeric<Dst>(1) << 1U);
  TEST_EXPECTED_VALUE(
      0, ClampedNumeric<Dst>(1) >> (IntegerBitsPlusSign<Dst>::value + 0U));
  TEST_EXPECTED_VALUE(
      0, ClampedNumeric<Dst>(1) >> (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(
      -1, ClampedNumeric<Dst>(-1) >> (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(-1, ClampedNumeric<Dst>(DstLimits::lowest()) >>
                              (IntegerBitsPlusSign<Dst>::value - 0U));

  TestStrictPointerMath<Dst>();
}

// Unsigned integer arithmetic.
template <typename Dst>
static void TestSpecializedArithmetic(
    const char* dst,
    int line,
    std::enable_if_t<numeric_limits<Dst>::is_integer &&
                         !numeric_limits<Dst>::is_signed,
                     int> = 0) {
  using DstLimits = SaturationDefaultLimits<Dst>;
  TEST_EXPECTED_SUCCESS(-CheckedNumeric<Dst>(DstLimits::lowest()));
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) - 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) * 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) / 2);
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_SUCCESS(
      CheckedNumeric<typename std::make_signed<Dst>::type>(
          std::numeric_limits<typename std::make_signed<Dst>::type>::lowest())
          .UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      MakeCheckedNum(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeCheckedNum(DstLimits::max()).UnsignedAbs());
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1).UnsignedAbs());

  TEST_EXPECTED_VALUE(0, -ClampedNumeric<Dst>(DstLimits::lowest()));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) - 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) * 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) / 2);
  TEST_EXPECTED_VALUE(0,
                      ClampedNumeric<Dst>(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_VALUE(
      as_unsigned(
          std::numeric_limits<typename std::make_signed<Dst>::type>::lowest()),
      ClampedNumeric<typename std::make_signed<Dst>::type>(
          std::numeric_limits<typename std::make_signed<Dst>::type>::lowest())
          .UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::lowest(),
                      MakeClampedNum(DstLimits::lowest()).UnsignedAbs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      MakeClampedNum(DstLimits::max()).UnsignedAbs());
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0).UnsignedAbs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1).UnsignedAbs());

  // Modulus is legal only for integers.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>() % 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) % 2);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::lowest()) % 2);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(DstLimits::max()) % 2);
  // Test all the different modulus combinations.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 % CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
  CheckedNumeric<Dst> checked_dst = 1;
  TEST_EXPECTED_VALUE(0, checked_dst %= 1);
  // Test that div by 0 is avoided but returns invalid result.
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) % 0);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1)
                        << IntegerBitsPlusSign<Dst>::value);
  // Test bit shifts.
  volatile int negative_one = -1;
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) << negative_one);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1)
                        << IntegerBitsPlusSign<Dst>::value);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(0)
                        << IntegerBitsPlusSign<Dst>::value);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) << 1);
  TEST_EXPECTED_VALUE(
      static_cast<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 1),
      CheckedNumeric<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 1));
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) << 0);
  TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) << 1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) >>
                        IntegerBitsPlusSign<Dst>::value);
  TEST_EXPECTED_VALUE(
      0, CheckedNumeric<Dst>(1) >> (IntegerBitsPlusSign<Dst>::value - 1));
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) >> negative_one);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) & 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) & 0);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) & 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) & 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      MakeCheckedNum(DstLimits::max()) & -1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) | 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) | 0);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(0) | 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) | 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      CheckedNumeric<Dst>(0) | static_cast<Dst>(-1));
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) ^ 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) ^ 0);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(0) ^ 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(0) ^ 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      CheckedNumeric<Dst>(0) ^ static_cast<Dst>(-1));
  TEST_EXPECTED_VALUE(DstLimits::max(), ~CheckedNumeric<Dst>(0));

  // Modulus is legal only for integers.
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>() % 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) % 2);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::lowest()) % 2);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(DstLimits::max()) % 2);
  // Test all the different modulus combinations.
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 % ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) % 1);
  ClampedNumeric<Dst> clamped_dst = 1;
  TEST_EXPECTED_VALUE(0, clamped_dst %= 1);
  // Test that div by 0 is avoided but returns invalid result.
  TEST_EXPECTED_VALUE(Dst(1), ClampedNumeric<Dst>(1) % 0);
  // Test bit shifts.
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(1)
                          << as_unsigned(IntegerBitsPlusSign<Dst>::value));
  TEST_EXPECTED_VALUE(Dst(0), ClampedNumeric<Dst>(0) << as_unsigned(
                                  IntegerBitsPlusSign<Dst>::value));
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::max()) << 1U);
  TEST_EXPECTED_VALUE(
      static_cast<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 1U),
      ClampedNumeric<Dst>(1) << (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) << 0U);
  TEST_EXPECTED_VALUE(2, ClampedNumeric<Dst>(1) << 1U);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) >>
                             as_unsigned(IntegerBitsPlusSign<Dst>::value));
  TEST_EXPECTED_VALUE(
      0, ClampedNumeric<Dst>(1) >> (IntegerBitsPlusSign<Dst>::value - 1U));
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) & 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) & 0);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) & 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) & 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      MakeClampedNum(DstLimits::max()) & -1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) | 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) | 0);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(0) | 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) | 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      ClampedNumeric<Dst>(0) | static_cast<Dst>(-1));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) ^ 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) ^ 0);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(0) ^ 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) ^ 0);
  TEST_EXPECTED_VALUE(std::numeric_limits<Dst>::max(),
                      ClampedNumeric<Dst>(0) ^ static_cast<Dst>(-1));
  TEST_EXPECTED_VALUE(DstLimits::max(), ~ClampedNumeric<Dst>(0));

  TestStrictPointerMath<Dst>();
}

// Floating point arithmetic.
template <typename Dst>
void TestSpecializedArithmetic(
    const char* dst,
    int line,
    std::enable_if_t<numeric_limits<Dst>::is_iec559, int> = 0) {
  using DstLimits = SaturationDefaultLimits<Dst>;
  TEST_EXPECTED_SUCCESS(-CheckedNumeric<Dst>(DstLimits::lowest()));

  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(-1).Abs());

  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) + 1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) +
                        DstLimits::lowest());

  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) -
                        DstLimits::lowest());
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) -
                        DstLimits::max());

  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::lowest()) * 2);

  TEST_EXPECTED_VALUE(-0.5, CheckedNumeric<Dst>(-1.0) / 2);

  TEST_EXPECTED_VALUE(DstLimits::max(),
                      -ClampedNumeric<Dst>(DstLimits::lowest()));

  TEST_EXPECTED_VALUE(DstLimits::max(),
                      ClampedNumeric<Dst>(DstLimits::lowest()).Abs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(-1).Abs());

  TEST_EXPECTED_VALUE(DstLimits::lowest() - 1,
                      ClampedNumeric<Dst>(DstLimits::lowest()) + -1);
  TEST_EXPECTED_VALUE(DstLimits::max() + 1,
                      ClampedNumeric<Dst>(DstLimits::max()) + 1);
  TEST_EXPECTED_VALUE(
      DstLimits::Underflow(),
      ClampedNumeric<Dst>(DstLimits::lowest()) + DstLimits::lowest());

  TEST_EXPECTED_VALUE(
      DstLimits::Overflow(),
      ClampedNumeric<Dst>(DstLimits::max()) - DstLimits::lowest());
  TEST_EXPECTED_VALUE(
      DstLimits::Underflow(),
      ClampedNumeric<Dst>(DstLimits::lowest()) - DstLimits::max());

  TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                      ClampedNumeric<Dst>(DstLimits::lowest()) * 2);

  TEST_EXPECTED_VALUE(-0.5, ClampedNumeric<Dst>(-1.0) / 2);
}

// Generic arithmetic tests.
template <typename Dst>
static void TestArithmetic(const char* dst, int line) {
  using DstLimits = SaturationDefaultLimits<Dst>;

  // Test C++17 class template argument deduction
  static_assert(
      std::is_same_v<Dst, typename decltype(CheckedNumeric(Dst{0}))::type>);
  static_assert(
      std::is_same_v<Dst, typename decltype(ClampedNumeric(Dst{0}))::type>);
  static_assert(
      std::is_same_v<Dst, typename decltype(StrictNumeric(Dst{0}))::type>);

  EXPECT_EQ(true, CheckedNumeric<Dst>().IsValid());
  EXPECT_EQ(false, CheckedNumeric<Dst>(CheckedNumeric<Dst>(DstLimits::max()) *
                                       DstLimits::max())
                       .IsValid());
  EXPECT_EQ(static_cast<Dst>(0), CheckedNumeric<Dst>().ValueOrDie());
  EXPECT_EQ(static_cast<Dst>(0), CheckedNumeric<Dst>().ValueOrDefault(1));
  EXPECT_EQ(static_cast<Dst>(1),
            CheckedNumeric<Dst>(CheckedNumeric<Dst>(DstLimits::max()) *
                                DstLimits::max())
                .ValueOrDefault(1));

  // Test the operator combinations.
  TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) + CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) - CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) * CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(2, 1 + CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 - CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, 1 * CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, 1 / CheckedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) + 1);
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) - 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) * 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / 1);
  CheckedNumeric<Dst> checked_dst = 1;
  TEST_EXPECTED_VALUE(2, checked_dst += 1);
  checked_dst = 1;
  TEST_EXPECTED_VALUE(0, checked_dst -= 1);
  checked_dst = 1;
  TEST_EXPECTED_VALUE(1, checked_dst *= 1);
  checked_dst = 1;
  TEST_EXPECTED_VALUE(1, checked_dst /= 1);

  TEST_EXPECTED_VALUE(2, ClampedNumeric<Dst>(1) + ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) - ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) * ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) / ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(2, 1 + ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(0, 1 - ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, 1 * ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(1, 1 / ClampedNumeric<Dst>(1));
  TEST_EXPECTED_VALUE(2, ClampedNumeric<Dst>(1) + 1);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(1) - 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) * 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) / 1);
  ClampedNumeric<Dst> clamped_dst = 1;
  TEST_EXPECTED_VALUE(2, clamped_dst += 1);
  clamped_dst = 1;
  TEST_EXPECTED_VALUE(0, clamped_dst -= 1);
  clamped_dst = 1;
  TEST_EXPECTED_VALUE(1, clamped_dst *= 1);
  clamped_dst = 1;
  TEST_EXPECTED_VALUE(1, clamped_dst /= 1);

  // Generic negation.
  if (DstLimits::is_signed) {
    TEST_EXPECTED_VALUE(0, -CheckedNumeric<Dst>());
    TEST_EXPECTED_VALUE(-1, -CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, -CheckedNumeric<Dst>(-1));
    TEST_EXPECTED_VALUE(static_cast<Dst>(DstLimits::max() * -1),
                        -CheckedNumeric<Dst>(DstLimits::max()));

    TEST_EXPECTED_VALUE(0, -ClampedNumeric<Dst>());
    TEST_EXPECTED_VALUE(-1, -ClampedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, -ClampedNumeric<Dst>(-1));
    TEST_EXPECTED_VALUE(static_cast<Dst>(DstLimits::max() * -1),
                        -ClampedNumeric<Dst>(DstLimits::max()));

    // The runtime paths for saturated negation differ significantly from what
    // gets evaluated at compile-time. Making this test volatile forces the
    // compiler to generate code rather than fold constant expressions.
    volatile Dst value = Dst(0);
    TEST_EXPECTED_VALUE(0, -MakeClampedNum(value));
    value = Dst(1);
    TEST_EXPECTED_VALUE(-1, -MakeClampedNum(value));
    value = Dst(2);
    TEST_EXPECTED_VALUE(-2, -MakeClampedNum(value));
    value = Dst(-1);
    TEST_EXPECTED_VALUE(1, -MakeClampedNum(value));
    value = Dst(-2);
    TEST_EXPECTED_VALUE(2, -MakeClampedNum(value));
    value = DstLimits::max();
    TEST_EXPECTED_VALUE(Dst(DstLimits::max() * -1), -MakeClampedNum(value));
    value = Dst(-1 * DstLimits::max());
    TEST_EXPECTED_VALUE(DstLimits::max(), -MakeClampedNum(value));
    value = DstLimits::lowest();
    TEST_EXPECTED_VALUE(DstLimits::max(), -MakeClampedNum(value));
  }

  // Generic absolute value.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>().Abs());
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1).Abs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      CheckedNumeric<Dst>(DstLimits::max()).Abs());

  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>().Abs());
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1).Abs());
  TEST_EXPECTED_VALUE(DstLimits::max(),
                      ClampedNumeric<Dst>(DstLimits::max()).Abs());

  // Generic addition.
  TEST_EXPECTED_VALUE(1, (CheckedNumeric<Dst>() + 1));
  TEST_EXPECTED_VALUE(2, (CheckedNumeric<Dst>(1) + 1));
  if (numeric_limits<Dst>::is_signed)
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(-1) + 1));
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::lowest()) + 1);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) +
                        DstLimits::max());

  TEST_EXPECTED_VALUE(1, (ClampedNumeric<Dst>() + 1));
  TEST_EXPECTED_VALUE(2, (ClampedNumeric<Dst>(1) + 1));
  if (numeric_limits<Dst>::is_signed)
    TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>(-1) + 1));
  TEST_EXPECTED_VALUE(DstLimits::lowest() + 1,
                      ClampedNumeric<Dst>(DstLimits::lowest()) + 1);
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::max()) + DstLimits::max());

  // Generic subtraction.
  TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(1) - 1));
  TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) - 1);
  if (numeric_limits<Dst>::is_signed) {
    TEST_EXPECTED_VALUE(-1, (CheckedNumeric<Dst>() - 1));
    TEST_EXPECTED_VALUE(-2, (CheckedNumeric<Dst>(-1) - 1));
  } else {
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) - -1);
  }

  TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>(1) - 1));
  TEST_EXPECTED_VALUE(DstLimits::max() - 1,
                      ClampedNumeric<Dst>(DstLimits::max()) - 1);
  if (numeric_limits<Dst>::is_signed) {
    TEST_EXPECTED_VALUE(-1, (ClampedNumeric<Dst>() - 1));
    TEST_EXPECTED_VALUE(-2, (ClampedNumeric<Dst>(-1) - 1));
  } else {
    TEST_EXPECTED_VALUE(DstLimits::max(),
                        ClampedNumeric<Dst>(DstLimits::max()) - -1);
  }

  // Generic multiplication.
  TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>() * 1));
  TEST_EXPECTED_VALUE(1, (CheckedNumeric<Dst>(1) * 1));
  TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(0) * 0));
  if (numeric_limits<Dst>::is_signed) {
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(-1) * 0));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(0) * -1));
    TEST_EXPECTED_VALUE(-2, (CheckedNumeric<Dst>(-1) * 2));
  } else {
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) * -2);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) *
                          CheckedNumeric<uintmax_t>(-2));
  }
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) *
                        DstLimits::max());

  TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>() * 1));
  TEST_EXPECTED_VALUE(1, (ClampedNumeric<Dst>(1) * 1));
  TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>(0) * 0));
  if (numeric_limits<Dst>::is_signed) {
    TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>(-1) * 0));
    TEST_EXPECTED_VALUE(0, (ClampedNumeric<Dst>(0) * -1));
    TEST_EXPECTED_VALUE(-2, (ClampedNumeric<Dst>(-1) * 2));
  } else {
    TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                        ClampedNumeric<Dst>(DstLimits::max()) * -2);
    TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(DstLimits::max()) *
                               ClampedNumeric<uintmax_t>(-2));
  }
  TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                      ClampedNumeric<Dst>(DstLimits::max()) * DstLimits::max());

  // Generic division.
  TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>() / 1);
  TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / 1);
  TEST_EXPECTED_VALUE(DstLimits::lowest() / 2,
                      CheckedNumeric<Dst>(DstLimits::lowest()) / 2);
  TEST_EXPECTED_VALUE(DstLimits::max() / 2,
                      CheckedNumeric<Dst>(DstLimits::max()) / 2);
  TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(1) / 0);

  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>() / 1);
  TEST_EXPECTED_VALUE(1, ClampedNumeric<Dst>(1) / 1);
  TEST_EXPECTED_VALUE(DstLimits::lowest() / 2,
                      ClampedNumeric<Dst>(DstLimits::lowest()) / 2);
  TEST_EXPECTED_VALUE(DstLimits::max() / 2,
                      ClampedNumeric<Dst>(DstLimits::max()) / 2);
  TEST_EXPECTED_VALUE(DstLimits::Overflow(), ClampedNumeric<Dst>(1) / 0);
  TEST_EXPECTED_VALUE(DstLimits::Underflow(), ClampedNumeric<Dst>(-1) / 0);
  TEST_EXPECTED_VALUE(0, ClampedNumeric<Dst>(0) / 0);

  TestSpecializedArithmetic<Dst>(dst, line);
}

// Helper macro to wrap displaying the conversion types and line numbers.
#define TEST_ARITHMETIC(Dst) TestArithmetic<Dst>(#Dst, __LINE__)

TEST(SafeNumerics, SignedIntegerMath) {
  TEST_ARITHMETIC(int8_t);
  TEST_ARITHMETIC(int16_t);
  TEST_ARITHMETIC(int);
  TEST_ARITHMETIC(intptr_t);
  TEST_ARITHMETIC(intmax_t);
}

TEST(SafeNumerics, UnsignedIntegerMath) {
  TEST_ARITHMETIC(uint8_t);
  TEST_ARITHMETIC(uint16_t);
  TEST_ARITHMETIC(unsigned int);
  TEST_ARITHMETIC(uintptr_t);
  TEST_ARITHMETIC(uintmax_t);
}

TEST(SafeNumerics, FloatingPointMath) {
  TEST_ARITHMETIC(float);
  TEST_ARITHMETIC(double);
}

// Enumerates the five different conversions types we need to test.
enum NumericConversionType {
  SIGN_PRESERVING_VALUE_PRESERVING,
  SIGN_PRESERVING_NARROW,
  SIGN_TO_UNSIGN_WIDEN_OR_EQUAL,
  SIGN_TO_UNSIGN_NARROW,
  UNSIGN_TO_SIGN_NARROW_OR_EQUAL,
};

// Template covering the different conversion tests.
template <typename Dst, typename Src, NumericConversionType conversion>
struct TestNumericConversion {};

enum RangeConstraint {
  RANGE_VALID = 0x0,      // Value can be represented by the destination type.
  RANGE_UNDERFLOW = 0x1,  // Value would underflow.
  RANGE_OVERFLOW = 0x2,   // Value would overflow.
  RANGE_INVALID = RANGE_UNDERFLOW | RANGE_OVERFLOW  // Invalid (i.e. NaN).
};

// These are some wrappers to make the tests a bit cleaner.
constexpr RangeConstraint RangeCheckToEnum(const RangeCheck constraint) {
  return static_cast<RangeConstraint>(
      static_cast<int>(constraint.IsOverflowFlagSet()) << 1 |
      static_cast<int>(constraint.IsUnderflowFlagSet()));
}

// EXPECT_EQ wrappers providing specific detail on test failures.
#define TEST_EXPECTED_RANGE(expected, actual)                               \
  EXPECT_EQ(expected,                                                       \
            RangeCheckToEnum(DstRangeRelationToSrcRange<Dst>(actual)))      \
      << "Conversion test: " << src << " value " << actual << " to " << dst \
      << " on line " << line

template <typename Dst, typename Src>
void TestStrictComparison(const char* dst, const char* src, int line) {
  using DstLimits = numeric_limits<Dst>;
  using SrcLimits = numeric_limits<Src>;
  static_assert(StrictNumeric<Src>(SrcLimits::lowest()) < DstLimits::max(), "");
  static_assert(StrictNumeric<Src>(SrcLimits::lowest()) < SrcLimits::max(), "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::lowest()) >= DstLimits::max()),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::lowest()) >= SrcLimits::max()),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::lowest()) <= DstLimits::max(),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::lowest()) <= SrcLimits::max(),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::lowest()) > DstLimits::max()),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::lowest()) > SrcLimits::max()),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) > DstLimits::lowest(), "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) > SrcLimits::lowest(), "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::max()) <= DstLimits::lowest()),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::max()) <= SrcLimits::lowest()),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) >= DstLimits::lowest(),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) >= SrcLimits::lowest(),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::max()) < DstLimits::lowest()),
                "");
  static_assert(!(StrictNumeric<Src>(SrcLimits::max()) < SrcLimits::lowest()),
                "");
  static_assert(StrictNumeric<Src>(static_cast<Src>(1)) == static_cast<Dst>(1),
                "");
  static_assert(StrictNumeric<Src>(static_cast<Src>(1)) != static_cast<Dst>(0),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) != static_cast<Dst>(0),
                "");
  static_assert(StrictNumeric<Src>(SrcLimits::max()) != DstLimits::lowest(),
                "");
  static_assert(
      !(StrictNumeric<Src>(static_cast<Src>(1)) != static_cast<Dst>(1)), "");
  static_assert(
      !(StrictNumeric<Src>(static_cast<Src>(1)) == static_cast<Dst>(0)), "");

  // Due to differences in float handling between compilers, these aren't
  // compile-time constants everywhere. So, we use run-time tests.
  EXPECT_EQ(
      SrcLimits::max(),
      MakeCheckedNum(SrcLimits::max()).Max(DstLimits::lowest()).ValueOrDie());
  EXPECT_EQ(
      DstLimits::max(),
      MakeCheckedNum(SrcLimits::lowest()).Max(DstLimits::max()).ValueOrDie());
  EXPECT_EQ(
      DstLimits::lowest(),
      MakeCheckedNum(SrcLimits::max()).Min(DstLimits::lowest()).ValueOrDie());
  EXPECT_EQ(
      SrcLimits::lowest(),
      MakeCheckedNum(SrcLimits::lowest()).Min(DstLimits::max()).ValueOrDie());
  EXPECT_EQ(SrcLimits::lowest(), CheckMin(MakeStrictNum(1), MakeCheckedNum(0),
                                          DstLimits::max(), SrcLimits::lowest())
                                     .ValueOrDie());
  EXPECT_EQ(DstLimits::max(), CheckMax(MakeStrictNum(1), MakeCheckedNum(0),
                                       DstLimits::max(), SrcLimits::lowest())
                                  .ValueOrDie());

  EXPECT_EQ(SrcLimits::max(),
            MakeClampedNum(SrcLimits::max()).Max(DstLimits::lowest()));
  EXPECT_EQ(DstLimits::max(),
            MakeClampedNum(SrcLimits::lowest()).Max(DstLimits::max()));
  EXPECT_EQ(DstLimits::lowest(),
            MakeClampedNum(SrcLimits::max()).Min(DstLimits::lowest()));
  EXPECT_EQ(SrcLimits::lowest(),
            MakeClampedNum(SrcLimits::lowest()).Min(DstLimits::max()));
  EXPECT_EQ(SrcLimits::lowest(),
            ClampMin(MakeStrictNum(1), MakeClampedNum(0), DstLimits::max(),
                     SrcLimits::lowest()));
  EXPECT_EQ(DstLimits::max(), ClampMax(MakeStrictNum(1), MakeClampedNum(0),
                                       DstLimits::max(), SrcLimits::lowest()));

  if (IsValueInRangeForNumericType<Dst>(SrcLimits::max())) {
    TEST_EXPECTED_VALUE(Dst(SrcLimits::max()), (CommonMax<Dst, Src>()));
    TEST_EXPECTED_VALUE(Dst(SrcLimits::max()),
                        (CommonMaxOrMin<Dst, Src>(false)));
  } else {
    TEST_EXPECTED_VALUE(DstLimits::max(), (CommonMax<Dst, Src>()));
    TEST_EXPECTED_VALUE(DstLimits::max(), (CommonMaxOrMin<Dst, Src>(false)));
  }

  if (IsValueInRangeForNumericType<Dst>(SrcLimits::lowest())) {
    TEST_EXPECTED_VALUE(Dst(SrcLimits::lowest()), (CommonMin<Dst, Src>()));
    TEST_EXPECTED_VALUE(Dst(SrcLimits::lowest()),
                        (CommonMaxOrMin<Dst, Src>(true)));
  } else {
    TEST_EXPECTED_VALUE(DstLimits::lowest(), (CommonMin<Dst, Src>()));
    TEST_EXPECTED_VALUE(DstLimits::lowest(), (CommonMaxOrMin<Dst, Src>(true)));
  }
}

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_VALUE_PRESERVING> {
  static void Test(const char* dst, const char* src, int line) {
    using SrcLimits = SaturationDefaultLimits<Src>;
    using DstLimits = SaturationDefaultLimits<Dst>;
    // Integral to floating.
    static_assert((DstLimits::is_iec559 && SrcLimits::is_integer) ||
                      // Not floating to integral and...
                      (!(DstLimits::is_integer && SrcLimits::is_iec559) &&
                       // Same sign, same numeric, source is narrower or same.
                       ((SrcLimits::is_signed == DstLimits::is_signed &&
                         MaxExponent<Dst>::value >= MaxExponent<Src>::value) ||
                        // Or signed destination and source is smaller
                        (DstLimits::is_signed &&
                         MaxExponent<Dst>::value >= MaxExponent<Src>::value))),
                  "Comparison must be sign preserving and value preserving");

    TestStrictComparison<Dst, Src>(dst, src, line);

    const CheckedNumeric<Dst> checked_dst = SrcLimits::max();
    const ClampedNumeric<Dst> clamped_dst = SrcLimits::max();
    TEST_EXPECTED_SUCCESS(checked_dst);
    TEST_EXPECTED_VALUE(Dst(SrcLimits::max()), clamped_dst);
    if (MaxExponent<Dst>::value > MaxExponent<Src>::value) {
      if (MaxExponent<Dst>::value >= MaxExponent<Src>::value * 2 - 1) {
        // At least twice larger type.
        TEST_EXPECTED_SUCCESS(SrcLimits::max() * checked_dst);
        TEST_EXPECTED_VALUE(SrcLimits::max() * clamped_dst,
                            Dst(SrcLimits::max()) * Dst(SrcLimits::max()));
      } else {  // Larger, but not at least twice as large.
        TEST_EXPECTED_FAILURE(SrcLimits::max() * checked_dst);
        TEST_EXPECTED_SUCCESS(checked_dst + 1);
        TEST_EXPECTED_VALUE(DstLimits::Overflow(),
                            SrcLimits::max() * clamped_dst);
        TEST_EXPECTED_VALUE(Dst(SrcLimits::max()) + Dst(1),
                            clamped_dst + Dst(1));
      }
    } else {  // Same width type.
      TEST_EXPECTED_FAILURE(checked_dst + 1);
      TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst + Dst(1));
    }

    TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max());
    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
    if (SrcLimits::is_iec559) {
      TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max() * static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
    } else if (numeric_limits<Src>::is_signed) {
      // This block reverses the Src to Dst relationship so we don't have to
      // complicate the test macros.
      if (!std::is_same_v<Src, Dst>) {
        TEST_EXPECTED_SUCCESS(CheckDiv(SrcLimits::lowest(), Dst(-1)));
      }
      TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::lowest());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_NARROW> {
  static void Test(const char* dst, const char* src, int line) {
    using SrcLimits = SaturationDefaultLimits<Src>;
    using DstLimits = SaturationDefaultLimits<Dst>;
    static_assert(SrcLimits::is_signed == DstLimits::is_signed,
                  "Destination and source sign must be the same");
    static_assert(MaxExponent<Dst>::value <= MaxExponent<Src>::value,
                  "Destination must be narrower than source");

    TestStrictComparison<Dst, Src>(dst, src, line);

    const CheckedNumeric<Dst> checked_dst;
    TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(1, checked_dst + Src(1));
    TEST_EXPECTED_FAILURE(checked_dst - SrcLimits::max());

    ClampedNumeric<Dst> clamped_dst;
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(1, clamped_dst + Src(1));
    TEST_EXPECTED_VALUE(DstLimits::Underflow(), clamped_dst - SrcLimits::max());
    clamped_dst += SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst);
    clamped_dst = DstLimits::max();
    clamped_dst += SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst);
    clamped_dst = DstLimits::max();
    clamped_dst -= SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Underflow(), clamped_dst);
    clamped_dst = 0;

    TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
    if (SrcLimits::is_iec559) {
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::max() * -1);
      TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
      TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
      if (DstLimits::is_integer) {
        if (SrcLimits::digits < DstLimits::digits) {
          TEST_EXPECTED_RANGE(RANGE_OVERFLOW,
                              static_cast<Src>(DstLimits::max()));
        } else {
          TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::max()));
        }
        TEST_EXPECTED_RANGE(
            RANGE_VALID,
            static_cast<Src>(GetMaxConvertibleToFloat<Src, Dst>()));
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::lowest()));
      }
    } else if (SrcLimits::is_signed) {
      TEST_EXPECTED_VALUE(-1, checked_dst - static_cast<Src>(1));
      TEST_EXPECTED_VALUE(-1, clamped_dst - static_cast<Src>(1));
      TEST_EXPECTED_VALUE(Src(Src(0) - DstLimits::lowest()),
                          ClampDiv(DstLimits::lowest(), Src(-1)));
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::lowest());
      TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
    } else {
      TEST_EXPECTED_FAILURE(checked_dst - static_cast<Src>(1));
      TEST_EXPECTED_VALUE(Dst(0), clamped_dst - static_cast<Src>(1));
      TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::lowest());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL> {
  static void Test(const char* dst, const char* src, int line) {
    using SrcLimits = SaturationDefaultLimits<Src>;
    using DstLimits = SaturationDefaultLimits<Dst>;
    static_assert(MaxExponent<Dst>::value >= MaxExponent<Src>::value,
                  "Destination must be equal or wider than source.");
    static_assert(SrcLimits::is_signed, "Source must be signed");
    static_assert(!DstLimits::is_signed, "Destination must be unsigned");

    TestStrictComparison<Dst, Src>(dst, src, line);

    const CheckedNumeric<Dst> checked_dst;
    TEST_EXPECTED_VALUE(SrcLimits::max(), checked_dst + SrcLimits::max());
    TEST_EXPECTED_FAILURE(checked_dst + static_cast<Src>(-1));
    TEST_EXPECTED_SUCCESS(checked_dst * static_cast<Src>(-1));
    TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::lowest());
    TEST_EXPECTED_VALUE(Dst(0), CheckDiv(Dst(0), Src(-1)));

    const ClampedNumeric<Dst> clamped_dst;
    TEST_EXPECTED_VALUE(SrcLimits::max(), clamped_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                        clamped_dst + static_cast<Src>(-1));
    TEST_EXPECTED_VALUE(0, clamped_dst * static_cast<Src>(-1));
    TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                        clamped_dst + SrcLimits::lowest());

    TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::lowest());
    TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max());
    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
    TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, static_cast<Src>(-1));
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_NARROW> {
  static void Test(const char* dst, const char* src, int line) {
    using SrcLimits = SaturationDefaultLimits<Src>;
    using DstLimits = SaturationDefaultLimits<Dst>;
    static_assert(MaxExponent<Dst>::value < MaxExponent<Src>::value,
                  "Destination must be narrower than source.");
    static_assert(SrcLimits::is_signed, "Source must be signed.");
    static_assert(!DstLimits::is_signed, "Destination must be unsigned.");

    TestStrictComparison<Dst, Src>(dst, src, line);

    const CheckedNumeric<Dst> checked_dst;
    TEST_EXPECTED_VALUE(1, checked_dst + static_cast<Src>(1));
    TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
    TEST_EXPECTED_FAILURE(checked_dst + static_cast<Src>(-1));
    TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::lowest());

    ClampedNumeric<Dst> clamped_dst;
    TEST_EXPECTED_VALUE(1, clamped_dst + static_cast<Src>(1));
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                        clamped_dst + static_cast<Src>(-1));
    TEST_EXPECTED_VALUE(DstLimits::Underflow(),
                        clamped_dst + SrcLimits::lowest());
    clamped_dst += SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst);
    clamped_dst = DstLimits::max();
    clamped_dst += SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst);
    clamped_dst = DstLimits::max();
    clamped_dst -= SrcLimits::max();
    TEST_EXPECTED_VALUE(DstLimits::Underflow(), clamped_dst);
    clamped_dst = 0;

    TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
    TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, static_cast<Src>(-1));

    // Additional saturation tests.
    EXPECT_EQ(DstLimits::max(), saturated_cast<Dst>(SrcLimits::max()));
    EXPECT_EQ(DstLimits::lowest(), saturated_cast<Dst>(SrcLimits::lowest()));

    if (SrcLimits::is_iec559) {
      EXPECT_EQ(Dst(0), saturated_cast<Dst>(SrcLimits::quiet_NaN()));

      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::max() * -1);
      TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
      TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
      if (DstLimits::is_integer) {
        if (SrcLimits::digits < DstLimits::digits) {
          TEST_EXPECTED_RANGE(RANGE_OVERFLOW,
                              static_cast<Src>(DstLimits::max()));
        } else {
          TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::max()));
        }
        TEST_EXPECTED_RANGE(
            RANGE_VALID,
            static_cast<Src>(GetMaxConvertibleToFloat<Src, Dst>()));
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::lowest()));
      }
    } else {
      TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::lowest());
    }
  }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, UNSIGN_TO_SIGN_NARROW_OR_EQUAL> {
  static void Test(const char* dst, const char* src, int line) {
    using SrcLimits = SaturationDefaultLimits<Src>;
    using DstLimits = SaturationDefaultLimits<Dst>;
    static_assert(MaxExponent<Dst>::value <= MaxExponent<Src>::value,
                  "Destination must be narrower or equal to source.");
    static_assert(!SrcLimits::is_signed, "Source must be unsigned.");
    static_assert(DstLimits::is_signed, "Destination must be signed.");

    TestStrictComparison<Dst, Src>(dst, src, line);

    const CheckedNumeric<Dst> checked_dst;
    TEST_EXPECTED_VALUE(1, checked_dst + static_cast<Src>(1));
    TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(SrcLimits::lowest(), checked_dst + SrcLimits::lowest());

    const ClampedNumeric<Dst> clamped_dst;
    TEST_EXPECTED_VALUE(1, clamped_dst + static_cast<Src>(1));
    TEST_EXPECTED_VALUE(DstLimits::Overflow(), clamped_dst + SrcLimits::max());
    TEST_EXPECTED_VALUE(SrcLimits::lowest(), clamped_dst + SrcLimits::lowest());

    TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::lowest());
    TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));

    // Additional saturation tests.
    EXPECT_EQ(DstLimits::max(), saturated_cast<Dst>(SrcLimits::max()));
    EXPECT_EQ(Dst(0), saturated_cast<Dst>(SrcLimits::lowest()));
  }
};

// Helper macro to wrap displaying the conversion types and line numbers
#define TEST_NUMERIC_CONVERSION(d, s, t) \
  TestNumericConversion<d, s, t>::Test(#d, #s, __LINE__)

TEST(SafeNumerics, IntMinOperations) {
  TEST_NUMERIC_CONVERSION(int8_t, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uint8_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(int8_t, int16_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int8_t, int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, uint16_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, unsigned int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int8_t, float, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(uint8_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(uint8_t, int16_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, int, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, intmax_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint8_t, float, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(int8_t, uint16_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int8_t, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int8_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, Int16Operations) {
  TEST_NUMERIC_CONVERSION(int16_t, int16_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uint16_t, uint16_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(int16_t, int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(uint16_t, unsigned int, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int16_t, float, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(uint16_t, int16_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(uint16_t, int, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint16_t, intmax_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uint16_t, float, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(int16_t, unsigned int,
                          UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int16_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntOperations) {
  TEST_NUMERIC_CONVERSION(int, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(unsigned int, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(int, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(unsigned int, uint8_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(int, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(int, intmax_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, uintmax_t, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int, float, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(int, double, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(unsigned int, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(unsigned int, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(unsigned int, intmax_t, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, float, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(unsigned int, double, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(int, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntMaxOperations) {
  TEST_NUMERIC_CONVERSION(intmax_t, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uintmax_t, uintmax_t,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(uintmax_t, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(intmax_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(intmax_t, float, SIGN_PRESERVING_NARROW);
  TEST_NUMERIC_CONVERSION(intmax_t, double, SIGN_PRESERVING_NARROW);

  TEST_NUMERIC_CONVERSION(uintmax_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(uintmax_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

  TEST_NUMERIC_CONVERSION(uintmax_t, float, SIGN_TO_UNSIGN_NARROW);
  TEST_NUMERIC_CONVERSION(uintmax_t, double, SIGN_TO_UNSIGN_NARROW);

  TEST_NUMERIC_CONVERSION(intmax_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, FloatOperations) {
  TEST_NUMERIC_CONVERSION(float, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, uintmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(float, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);

  TEST_NUMERIC_CONVERSION(float, double, SIGN_PRESERVING_NARROW);
}

TEST(SafeNumerics, DoubleOperations) {
  TEST_NUMERIC_CONVERSION(double, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, uintmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, int, SIGN_PRESERVING_VALUE_PRESERVING);
  TEST_NUMERIC_CONVERSION(double, unsigned int,
                          SIGN_PRESERVING_VALUE_PRESERVING);
}

TEST(SafeNumerics, SizeTOperations) {
  TEST_NUMERIC_CONVERSION(size_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
  TEST_NUMERIC_CONVERSION(int, size_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

// A one-off test to ensure StrictNumeric won't resolve to an incorrect type.
// If this fails we'll just get a compiler error on an ambiguous overload.
int TestOverload(int) {  // Overload fails.
  return 0;
}
uint8_t TestOverload(uint8_t) {  // Overload fails.
  return 0;
}
size_t TestOverload(size_t) {  // Overload succeeds.
  return 0;
}

static_assert(std::is_same_v<decltype(TestOverload(StrictNumeric<int>())), int>,
              "");
static_assert(
    std::is_same_v<decltype(TestOverload(StrictNumeric<size_t>())), size_t>,
    "");

template <typename T>
struct CastTest1 {
  static constexpr T NaN() { return -1; }
  static constexpr T max() { return numeric_limits<T>::max() - 1; }
  static constexpr T Overflow() { return max(); }
  static constexpr T lowest() { return numeric_limits<T>::lowest() + 1; }
  static constexpr T Underflow() { return lowest(); }
};

template <typename T>
struct CastTest2 {
  static constexpr T NaN() { return 11; }
  static constexpr T max() { return 10; }
  static constexpr T Overflow() { return max(); }
  static constexpr T lowest() { return 1; }
  static constexpr T Underflow() { return lowest(); }
};

TEST(SafeNumerics, CastTests) {
// MSVC catches and warns that we're forcing saturation in these tests.
// Since that's intentional, we need to shut this warning off.
#if defined(COMPILER_MSVC)
#pragma warning(disable : 4756)
#endif

  int small_positive = 1;
  int small_negative = -1;
  double double_small = 1.0;
  double double_large = numeric_limits<double>::max();
  double double_infinity = numeric_limits<float>::infinity();
  double double_large_int = numeric_limits<int>::max();
  double double_small_int = numeric_limits<int>::lowest();

  // Just test that the casts compile, since the other tests cover logic.
  EXPECT_EQ(0, checked_cast<int>(static_cast<size_t>(0)));
  EXPECT_EQ(0, strict_cast<int>(static_cast<char>(0)));
  EXPECT_EQ(0, strict_cast<int>(static_cast<unsigned char>(0)));
  EXPECT_EQ(0U, strict_cast<unsigned>(static_cast<unsigned char>(0)));
  EXPECT_EQ(1ULL, static_cast<uint64_t>(StrictNumeric<size_t>(1U)));
  EXPECT_EQ(1ULL, static_cast<uint64_t>(SizeT(1U)));
  EXPECT_EQ(1U, static_cast<size_t>(StrictNumeric<unsigned>(1U)));

  EXPECT_TRUE(CheckedNumeric<uint64_t>(StrictNumeric<unsigned>(1U)).IsValid());
  EXPECT_TRUE(CheckedNumeric<int>(StrictNumeric<unsigned>(1U)).IsValid());
  EXPECT_FALSE(CheckedNumeric<unsigned>(StrictNumeric<int>(-1)).IsValid());

  EXPECT_TRUE(IsValueNegative(-1));
  EXPECT_TRUE(IsValueNegative(numeric_limits<int>::lowest()));
  EXPECT_FALSE(IsValueNegative(numeric_limits<unsigned>::lowest()));
  EXPECT_TRUE(IsValueNegative(numeric_limits<double>::lowest()));
  EXPECT_FALSE(IsValueNegative(0));
  EXPECT_FALSE(IsValueNegative(1));
  EXPECT_FALSE(IsValueNegative(0u));
  EXPECT_FALSE(IsValueNegative(1u));
  EXPECT_FALSE(IsValueNegative(numeric_limits<int>::max()));
  EXPECT_FALSE(IsValueNegative(numeric_limits<unsigned>::max()));
  EXPECT_FALSE(IsValueNegative(numeric_limits<double>::max()));

  // These casts and coercions will fail to compile:
  // EXPECT_EQ(0, strict_cast<int>(static_cast<size_t>(0)));
  // EXPECT_EQ(0, strict_cast<size_t>(static_cast<int>(0)));
  // EXPECT_EQ(1ULL, StrictNumeric<size_t>(1));
  // EXPECT_EQ(1, StrictNumeric<size_t>(1U));

  // Test various saturation corner cases.
  EXPECT_EQ(saturated_cast<int>(small_negative),
            static_cast<int>(small_negative));
  EXPECT_EQ(saturated_cast<int>(small_positive),
            static_cast<int>(small_positive));
  EXPECT_EQ(saturated_cast<unsigned>(small_negative), static_cast<unsigned>(0));
  EXPECT_EQ(saturated_cast<int>(double_small), static_cast<int>(double_small));
  EXPECT_EQ(saturated_cast<int>(double_large), numeric_limits<int>::max());
  EXPECT_EQ(saturated_cast<float>(double_large), double_infinity);
  EXPECT_EQ(saturated_cast<float>(-double_large), -double_infinity);
  EXPECT_EQ(numeric_limits<int>::lowest(),
            saturated_cast<int>(double_small_int));
  EXPECT_EQ(numeric_limits<int>::max(), saturated_cast<int>(double_large_int));

  // Test the saturated cast overrides.
  using FloatLimits = numeric_limits<float>;
  using IntLimits = numeric_limits<int>;
  EXPECT_EQ(-1, (saturated_cast<int, CastTest1>(FloatLimits::quiet_NaN())));
  EXPECT_EQ(CastTest1<int>::max(),
            (saturated_cast<int, CastTest1>(FloatLimits::infinity())));
  EXPECT_EQ(CastTest1<int>::max(),
            (saturated_cast<int, CastTest1>(FloatLimits::max())));
  EXPECT_EQ(CastTest1<int>::max(),
            (saturated_cast<int, CastTest1>(float(IntLimits::max()))));
  EXPECT_EQ(CastTest1<int>::lowest(),
            (saturated_cast<int, CastTest1>(-FloatLimits::infinity())));
  EXPECT_EQ(CastTest1<int>::lowest(),
            (saturated_cast<int, CastTest1>(FloatLimits::lowest())));
  EXPECT_EQ(0, (saturated_cast<int, CastTest1>(0.0)));
  EXPECT_EQ(1, (saturated_cast<int, CastTest1>(1.0)));
  EXPECT_EQ(-1, (saturated_cast<int, CastTest1>(-1.0)));
  EXPECT_EQ(0, (saturated_cast<int, CastTest1>(0)));
  EXPECT_EQ(1, (saturated_cast<int, CastTest1>(1)));
  EXPECT_EQ(-1, (saturated_cast<int, CastTest1>(-1)));
  EXPECT_EQ(CastTest1<int>::lowest(),
            (saturated_cast<int, CastTest1>(float(IntLimits::lowest()))));
  EXPECT_EQ(11, (saturated_cast<int, CastTest2>(FloatLimits::quiet_NaN())));
  EXPECT_EQ(10, (saturated_cast<int, CastTest2>(FloatLimits::infinity())));
  EXPECT_EQ(10, (saturated_cast<int, CastTest2>(FloatLimits::max())));
  EXPECT_EQ(1, (saturated_cast<int, CastTest2>(-FloatLimits::infinity())));
  EXPECT_EQ(1, (saturated_cast<int, CastTest2>(FloatLimits::lowest())));
  EXPECT_EQ(1, (saturated_cast<int, CastTest2>(0U)));

  float not_a_number = std::numeric_limits<float>::infinity() -
                       std::numeric_limits<float>::infinity();
  EXPECT_TRUE(std::isnan(not_a_number));
  EXPECT_EQ(0, saturated_cast<int>(not_a_number));

  // Test the CheckedNumeric value extractions functions.
  auto int8_min = MakeCheckedNum(numeric_limits<int8_t>::lowest());
  auto int8_max = MakeCheckedNum(numeric_limits<int8_t>::max());
  auto double_max = MakeCheckedNum(numeric_limits<double>::max());
  static_assert(
      std::is_same_v<int16_t, decltype(int8_min.ValueOrDie<int16_t>())::type>,
      "ValueOrDie returning incorrect type.");
  static_assert(
      std::is_same_v<int16_t,
                     decltype(int8_min.ValueOrDefault<int16_t>(0))::type>,
      "ValueOrDefault returning incorrect type.");
  EXPECT_FALSE(IsValidForType<uint8_t>(int8_min));
  EXPECT_TRUE(IsValidForType<uint8_t>(int8_max));
  EXPECT_EQ(static_cast<int>(numeric_limits<int8_t>::lowest()),
            ValueOrDieForType<int>(int8_min));
  EXPECT_TRUE(IsValidForType<uint32_t>(int8_max));
  EXPECT_EQ(static_cast<int>(numeric_limits<int8_t>::max()),
            ValueOrDieForType<int>(int8_max));
  EXPECT_EQ(0, ValueOrDefaultForType<int>(double_max, 0));
  uint8_t uint8_dest = 0;
  int16_t int16_dest = 0;
  double double_dest = 0;
  EXPECT_TRUE(int8_max.AssignIfValid(&uint8_dest));
  EXPECT_EQ(static_cast<uint8_t>(numeric_limits<int8_t>::max()), uint8_dest);
  EXPECT_FALSE(int8_min.AssignIfValid(&uint8_dest));
  EXPECT_TRUE(int8_max.AssignIfValid(&int16_dest));
  EXPECT_EQ(static_cast<int16_t>(numeric_limits<int8_t>::max()), int16_dest);
  EXPECT_TRUE(int8_min.AssignIfValid(&int16_dest));
  EXPECT_EQ(static_cast<int16_t>(numeric_limits<int8_t>::lowest()), int16_dest);
  EXPECT_FALSE(double_max.AssignIfValid(&uint8_dest));
  EXPECT_FALSE(double_max.AssignIfValid(&int16_dest));
  EXPECT_TRUE(double_max.AssignIfValid(&double_dest));
  EXPECT_EQ(numeric_limits<double>::max(), double_dest);
  EXPECT_EQ(1, checked_cast<int>(StrictNumeric<int>(1)));
  EXPECT_EQ(1, saturated_cast<int>(StrictNumeric<int>(1)));
  EXPECT_EQ(1, strict_cast<int>(StrictNumeric<int>(1)));

  enum class EnumTest { kOne = 1 };
  EXPECT_EQ(1, checked_cast<int>(EnumTest::kOne));
  EXPECT_EQ(1, saturated_cast<int>(EnumTest::kOne));
  EXPECT_EQ(1, strict_cast<int>(EnumTest::kOne));
}

TEST(SafeNumerics, IsValueInRangeForNumericType) {
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(0));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(1));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(2));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(-1));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(0xffffffffu));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0xffffffff)));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0x100000000)));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0x100000001)));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(
      std::numeric_limits<int32_t>::lowest()));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(
      std::numeric_limits<int64_t>::lowest()));

  // Converting to integer types will discard the fractional part first, so -0.9
  // will be truncated to -0.0.
  EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(-0.9));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(-1.0));

  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(1));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(2));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(-1));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0x7fffffff));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0x7fffffffu));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(0x80000000u));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(0xffffffffu));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0x80000000)));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0xffffffff)));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0x100000000)));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(
      std::numeric_limits<int32_t>::lowest()));
  EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(
      static_cast<int64_t>(std::numeric_limits<int32_t>::lowest())));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(
      static_cast<int64_t>(std::numeric_limits<int32_t>::lowest()) - 1));
  EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(
      std::numeric_limits<int64_t>::lowest()));

  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(0));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(1));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(2));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(-1));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(0xffffffffu));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0xffffffff)));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0x100000000)));
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0x100000001)));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(
      std::numeric_limits<int32_t>::lowest()));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(INT64_C(-1)));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(
      std::numeric_limits<int64_t>::lowest()));

  // Converting to integer types will discard the fractional part first, so -0.9
  // will be truncated to -0.0.
  EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(-0.9));
  EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(-1.0));

  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(1));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(2));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(-1));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x7fffffff));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x7fffffffu));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x80000000u));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0xffffffffu));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0x80000000)));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0xffffffff)));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0x100000000)));
  EXPECT_TRUE(
      IsValueInRangeForNumericType<int64_t>(INT64_C(0x7fffffffffffffff)));
  EXPECT_TRUE(
      IsValueInRangeForNumericType<int64_t>(UINT64_C(0x7fffffffffffffff)));
  EXPECT_FALSE(
      IsValueInRangeForNumericType<int64_t>(UINT64_C(0x8000000000000000)));
  EXPECT_FALSE(
      IsValueInRangeForNumericType<int64_t>(UINT64_C(0xffffffffffffffff)));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(
      std::numeric_limits<int32_t>::lowest()));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(
      static_cast<int64_t>(std::numeric_limits<int32_t>::lowest())));
  EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(
      std::numeric_limits<int64_t>::lowest()));
}

TEST(SafeNumerics, CompoundNumericOperations) {
  CheckedNumeric<int> a = 1;
  CheckedNumeric<int> b = 2;
  CheckedNumeric<int> c = 3;
  CheckedNumeric<int> d = 4;
  a += b;
  EXPECT_EQ(3, a.ValueOrDie());
  a -= c;
  EXPECT_EQ(0, a.ValueOrDie());
  d /= b;
  EXPECT_EQ(2, d.ValueOrDie());
  d *= d;
  EXPECT_EQ(4, d.ValueOrDie());
  d *= 0.5;
  EXPECT_EQ(2, d.ValueOrDie());

  CheckedNumeric<int> too_large = std::numeric_limits<int>::max();
  EXPECT_TRUE(too_large.IsValid());
  too_large += d;
  EXPECT_FALSE(too_large.IsValid());
  too_large -= d;
  EXPECT_FALSE(too_large.IsValid());
  too_large /= d;
  EXPECT_FALSE(too_large.IsValid());
}

TEST(SafeNumerics, TemplatedSafeMath) {
  // CheckMul and friends can be confusing, as they change behavior depending on
  // where the template is specified.
  uint64_t result;
  short short_one_thousand = 1000;
  // In this case, CheckMul uses template deduction to use the <short> variant,
  // and this will overflow even if assigned to a uint64_t.
  EXPECT_FALSE(CheckMul(short_one_thousand, short_one_thousand)
                   .AssignIfValid<uint64_t>(&result));
  EXPECT_FALSE(CheckMul(short_one_thousand, short_one_thousand).IsValid());
  // In both cases, CheckMul is forced to use the uint64_t template and will not
  // overflow.
  EXPECT_TRUE(CheckMul<uint64_t>(short_one_thousand, short_one_thousand)
                  .AssignIfValid(&result));
  EXPECT_TRUE(CheckMul<uint64_t>(short_one_thousand, short_one_thousand)
                  .AssignIfValid<uint64_t>(&result));

  uint64_t big_one_thousand = 1000u;
  // Order doesn't matter here: if one of the parameters is uint64_t then the
  // operation is done on a uint64_t.
  EXPECT_TRUE(
      CheckMul(big_one_thousand, short_one_thousand).AssignIfValid(&result));
  EXPECT_TRUE(
      CheckMul(short_one_thousand, big_one_thousand).AssignIfValid(&result));

  // Checked math functions can also take two template type parameters. Here are
  // the results of all four combinations.
  EXPECT_TRUE((CheckMul<short, uint64_t>(1000, 1000).AssignIfValid(&result)));

  // Note: Order here does not matter.
  EXPECT_TRUE((CheckMul<uint64_t, short>(1000, 1000).AssignIfValid(&result)));

  // Only if both are short will the operation be invalid.
  EXPECT_FALSE((CheckMul<short, short>(1000, 1000).AssignIfValid(&result)));

  // Same as above.
  EXPECT_TRUE(
      (CheckMul<uint64_t, uint64_t>(1000, 1000).AssignIfValid(&result)));
}

TEST(SafeNumerics, VariadicNumericOperations) {
  {  // Synthetic scope to avoid variable naming collisions.
    auto a = CheckAdd(1, 2UL, MakeCheckedNum(3LL), 4).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(a)::type>(10), a);
    auto b = CheckSub(MakeCheckedNum(20.0), 2UL, 4).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(b)::type>(14.0), b);
    auto c = CheckMul(20.0, MakeCheckedNum(1), 5, 3UL).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(c)::type>(300.0), c);
    auto d = CheckDiv(20.0, 2.0, MakeCheckedNum(5LL), -4).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(d)::type>(-.5), d);
    auto e = CheckMod(MakeCheckedNum(20), 3).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(e)::type>(2), e);
    auto f = CheckLsh(1, MakeCheckedNum(2)).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(f)::type>(4), f);
    auto g = CheckRsh(4, MakeCheckedNum(2)).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(g)::type>(1), g);
    auto h = CheckRsh(CheckAdd(1, 1, 1, 1), CheckSub(4, 2)).ValueOrDie();
    EXPECT_EQ(static_cast<decltype(h)::type>(1), h);
  }

  {
    auto a = ClampAdd(1, 2UL, MakeClampedNum(3LL), 4);
    EXPECT_EQ(static_cast<decltype(a)::type>(10), a);
    auto b = ClampSub(MakeClampedNum(20.0), 2UL, 4);
    EXPECT_EQ(static_cast<decltype(b)::type>(14.0), b);
    auto c = ClampMul(20.0, MakeClampedNum(1), 5, 3UL);
    EXPECT_EQ(static_cast<decltype(c)::type>(300.0), c);
    auto d = ClampDiv(20.0, 2.0, MakeClampedNum(5LL), -4);
    EXPECT_EQ(static_cast<decltype(d)::type>(-.5), d);
    auto e = ClampMod(MakeClampedNum(20), 3);
    EXPECT_EQ(static_cast<decltype(e)::type>(2), e);
    auto f = ClampLsh(1, MakeClampedNum(2U));
    EXPECT_EQ(static_cast<decltype(f)::type>(4), f);
    auto g = ClampRsh(4, MakeClampedNum(2U));
    EXPECT_EQ(static_cast<decltype(g)::type>(1), g);
    auto h = ClampRsh(ClampAdd(1, 1, 1, 1), ClampSub(4U, 2));
    EXPECT_EQ(static_cast<decltype(h)::type>(1), h);
  }
}

TEST(SafeNumerics, CeilInt) {
  constexpr float kMax = static_cast<float>(std::numeric_limits<int>::max());
  constexpr float kMin = std::numeric_limits<int>::min();
  constexpr float kInfinity = std::numeric_limits<float>::infinity();
  constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

  constexpr int kIntMax = std::numeric_limits<int>::max();
  constexpr int kIntMin = std::numeric_limits<int>::min();

  EXPECT_EQ(kIntMax, ClampCeil(kInfinity));
  EXPECT_EQ(kIntMax, ClampCeil(kMax));
  EXPECT_EQ(kIntMax, ClampCeil(kMax + 100.0f));
  EXPECT_EQ(0, ClampCeil(kNaN));

  EXPECT_EQ(-100, ClampCeil(-100.5f));
  EXPECT_EQ(0, ClampCeil(0.0f));
  EXPECT_EQ(101, ClampCeil(100.5f));

  EXPECT_EQ(kIntMin, ClampCeil(-kInfinity));
  EXPECT_EQ(kIntMin, ClampCeil(kMin));
  EXPECT_EQ(kIntMin, ClampCeil(kMin - 100.0f));
  EXPECT_EQ(0, ClampCeil(-kNaN));
}

TEST(SafeNumerics, FloorInt) {
  constexpr float kMax = static_cast<float>(std::numeric_limits<int>::max());
  constexpr float kMin = std::numeric_limits<int>::min();
  constexpr float kInfinity = std::numeric_limits<float>::infinity();
  constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

  constexpr int kIntMax = std::numeric_limits<int>::max();
  constexpr int kIntMin = std::numeric_limits<int>::min();

  EXPECT_EQ(kIntMax, ClampFloor(kInfinity));
  EXPECT_EQ(kIntMax, ClampFloor(kMax));
  EXPECT_EQ(kIntMax, ClampFloor(kMax + 100.0f));
  EXPECT_EQ(0, ClampFloor(kNaN));

  EXPECT_EQ(-101, ClampFloor(-100.5f));
  EXPECT_EQ(0, ClampFloor(0.0f));
  EXPECT_EQ(100, ClampFloor(100.5f));

  EXPECT_EQ(kIntMin, ClampFloor(-kInfinity));
  EXPECT_EQ(kIntMin, ClampFloor(kMin));
  EXPECT_EQ(kIntMin, ClampFloor(kMin - 100.0f));
  EXPECT_EQ(0, ClampFloor(-kNaN));
}

TEST(SafeNumerics, RoundInt) {
  constexpr float kMax = static_cast<float>(std::numeric_limits<int>::max());
  constexpr float kMin = std::numeric_limits<int>::min();
  constexpr float kInfinity = std::numeric_limits<float>::infinity();
  constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

  constexpr int kIntMax = std::numeric_limits<int>::max();
  constexpr int kIntMin = std::numeric_limits<int>::min();

  EXPECT_EQ(kIntMax, ClampRound(kInfinity));
  EXPECT_EQ(kIntMax, ClampRound(kMax));
  EXPECT_EQ(kIntMax, ClampRound(kMax + 100.0f));
  EXPECT_EQ(0, ClampRound(kNaN));

  EXPECT_EQ(-100, ClampRound(-100.1f));
  EXPECT_EQ(-101, ClampRound(-100.5f));
  EXPECT_EQ(-101, ClampRound(-100.9f));
  EXPECT_EQ(0, ClampRound(std::nextafter(-0.5f, 0.0f)));
  EXPECT_EQ(0, ClampRound(0.0f));
  EXPECT_EQ(0, ClampRound(std::nextafter(0.5f, 0.0f)));
  EXPECT_EQ(100, ClampRound(100.1f));
  EXPECT_EQ(101, ClampRound(100.5f));
  EXPECT_EQ(101, ClampRound(100.9f));

  EXPECT_EQ(kIntMin, ClampRound(-kInfinity));
  EXPECT_EQ(kIntMin, ClampRound(kMin));
  EXPECT_EQ(kIntMin, ClampRound(kMin - 100.0f));
  EXPECT_EQ(0, ClampRound(-kNaN));
}

TEST(SafeNumerics, Int64) {
  constexpr double kMax =
      static_cast<double>(std::numeric_limits<int64_t>::max());
  constexpr double kMin = std::numeric_limits<int64_t>::min();
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

  constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
  constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::min();

  EXPECT_EQ(kInt64Max, ClampFloor<int64_t>(kInfinity));
  EXPECT_EQ(kInt64Max, ClampCeil<int64_t>(kInfinity));
  EXPECT_EQ(kInt64Max, ClampRound<int64_t>(kInfinity));
  EXPECT_EQ(kInt64Max, ClampFloor<int64_t>(kMax));
  EXPECT_EQ(kInt64Max, ClampCeil<int64_t>(kMax));
  EXPECT_EQ(kInt64Max, ClampRound<int64_t>(kMax));
  EXPECT_EQ(kInt64Max, ClampFloor<int64_t>(kMax + 100.0));
  EXPECT_EQ(kInt64Max, ClampCeil<int64_t>(kMax + 100.0));
  EXPECT_EQ(kInt64Max, ClampRound<int64_t>(kMax + 100.0));
  EXPECT_EQ(0, ClampFloor<int64_t>(kNaN));
  EXPECT_EQ(0, ClampCeil<int64_t>(kNaN));
  EXPECT_EQ(0, ClampRound<int64_t>(kNaN));

  EXPECT_EQ(kInt64Min, ClampFloor<int64_t>(-kInfinity));
  EXPECT_EQ(kInt64Min, ClampCeil<int64_t>(-kInfinity));
  EXPECT_EQ(kInt64Min, ClampRound<int64_t>(-kInfinity));
  EXPECT_EQ(kInt64Min, ClampFloor<int64_t>(kMin));
  EXPECT_EQ(kInt64Min, ClampCeil<int64_t>(kMin));
  EXPECT_EQ(kInt64Min, ClampRound<int64_t>(kMin));
  EXPECT_EQ(kInt64Min, ClampFloor<int64_t>(kMin - 100.0));
  EXPECT_EQ(kInt64Min, ClampCeil<int64_t>(kMin - 100.0));
  EXPECT_EQ(kInt64Min, ClampRound<int64_t>(kMin - 100.0));
  EXPECT_EQ(0, ClampFloor<int64_t>(-kNaN));
  EXPECT_EQ(0, ClampCeil<int64_t>(-kNaN));
  EXPECT_EQ(0, ClampRound<int64_t>(-kNaN));
}

template <typename T>
void TestWrappingMathSigned() {
  static_assert(std::is_signed_v<T>);
  constexpr T kMinusTwo = -2;
  constexpr T kMinusOne = -1;
  constexpr T kZero = 0;
  constexpr T kOne = 1;
  constexpr T kTwo = 2;
  constexpr T kThree = 3;
  constexpr T kMax = std::numeric_limits<T>::max();
  constexpr T kMin = std::numeric_limits<T>::min();

  EXPECT_EQ(base::WrappingAdd(kOne, kTwo), kThree);
  static_assert(base::WrappingAdd(kOne, kTwo) == kThree);
  EXPECT_EQ(base::WrappingAdd(kMax, kOne), kMin);
  static_assert(base::WrappingAdd(kMax, kOne) == kMin);
  EXPECT_EQ(base::WrappingAdd(kMax, kTwo), kMin + 1);
  static_assert(base::WrappingAdd(kMax, kTwo) == kMin + 1);
  EXPECT_EQ(base::WrappingAdd(kMax, kMax), kMinusTwo);
  static_assert(base::WrappingAdd(kMax, kMax) == kMinusTwo);
  EXPECT_EQ(base::WrappingAdd(kMin, kMin), kZero);
  static_assert(base::WrappingAdd(kMin, kMin) == kZero);

  EXPECT_EQ(base::WrappingSub(kTwo, kOne), kOne);
  static_assert(base::WrappingSub(kTwo, kOne) == kOne);
  EXPECT_EQ(base::WrappingSub(kOne, kTwo), kMinusOne);
  static_assert(base::WrappingSub(kOne, kTwo) == kMinusOne);
  EXPECT_EQ(base::WrappingSub(kMin, kOne), kMax);
  static_assert(base::WrappingSub(kMin, kOne) == kMax);
  EXPECT_EQ(base::WrappingSub(kMin, kTwo), kMax - 1);
  static_assert(base::WrappingSub(kMin, kTwo) == kMax - 1);
  EXPECT_EQ(base::WrappingSub(kMax, kMin), kMinusOne);
  static_assert(base::WrappingSub(kMax, kMin) == kMinusOne);
  EXPECT_EQ(base::WrappingSub(kMin, kMax), kOne);
  static_assert(base::WrappingSub(kMin, kMax) == kOne);
}

template <typename T>
void TestWrappingMathUnsigned() {
  static_assert(std::is_unsigned_v<T>);
  constexpr T kZero = 0;
  constexpr T kOne = 1;
  constexpr T kTwo = 2;
  constexpr T kThree = 3;
  constexpr T kMax = std::numeric_limits<T>::max();

  EXPECT_EQ(base::WrappingAdd(kOne, kTwo), kThree);
  static_assert(base::WrappingAdd(kOne, kTwo) == kThree);
  EXPECT_EQ(base::WrappingAdd(kMax, kOne), kZero);
  static_assert(base::WrappingAdd(kMax, kOne) == kZero);
  EXPECT_EQ(base::WrappingAdd(kMax, kTwo), kOne);
  static_assert(base::WrappingAdd(kMax, kTwo) == kOne);
  EXPECT_EQ(base::WrappingAdd(kMax, kMax), kMax - 1);
  static_assert(base::WrappingAdd(kMax, kMax) == kMax - 1);

  EXPECT_EQ(base::WrappingSub(kTwo, kOne), kOne);
  static_assert(base::WrappingSub(kTwo, kOne) == kOne);
  EXPECT_EQ(base::WrappingSub(kOne, kTwo), kMax);
  static_assert(base::WrappingSub(kOne, kTwo) == kMax);
  EXPECT_EQ(base::WrappingSub(kZero, kOne), kMax);
  static_assert(base::WrappingSub(kZero, kOne) == kMax);
  EXPECT_EQ(base::WrappingSub(kZero, kTwo), kMax - 1);
  static_assert(base::WrappingSub(kZero, kTwo) == kMax - 1);
}

TEST(SafeNumerics, WrappingMath) {
  TestWrappingMathSigned<int8_t>();
  TestWrappingMathUnsigned<uint8_t>();
  TestWrappingMathSigned<int16_t>();
  TestWrappingMathUnsigned<uint16_t>();
  TestWrappingMathSigned<int32_t>();
  TestWrappingMathUnsigned<uint32_t>();
  TestWrappingMathSigned<int64_t>();
  TestWrappingMathUnsigned<uint64_t>();
}

TEST(SafeNumerics, StrictNumeric_SupportsAssignment) {
  StrictNumeric<uint16_t> val(uint16_t{5});
  EXPECT_EQ(static_cast<uint16_t>(val), 5u);

  // Same underlying type.
  val = uint16_t{6};
  EXPECT_EQ(static_cast<uint16_t>(val), 6u);

  // Different but strictly convertible type.
  val = uint8_t{7};
  EXPECT_EQ(static_cast<uint16_t>(val), 7u);

  // Same type.
  val = StrictNumeric<uint16_t>(uint16_t{8});
  EXPECT_EQ(static_cast<uint16_t>(val), 8u);

  // Different but strictly convertible type.
  val = StrictNumeric<uint8_t>(uint8_t{9});
  EXPECT_EQ(static_cast<uint16_t>(val), 9u);
}

#if defined(__clang__)
#pragma clang diagnostic pop  // -Winteger-overflow
#endif

}  // namespace internal
}  // namespace base
