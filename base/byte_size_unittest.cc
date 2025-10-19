// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_size.h"

#include <limits>
#include <sstream>

#include "base/rand_util.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Death tests are very slow, so by default only test a subset of boundary
// conditions.
constexpr bool kRunAllDeathTests = false;

// Boundaries of ByteSize and ByteSizeDelta.
constexpr uint64_t kMaxByteSize = std::numeric_limits<int64_t>::max();
constexpr int64_t kMaxByteSizeDelta = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinByteSizeDelta = std::numeric_limits<int64_t>::min();

// The min two's complement integer has no corresponding positive value, so also
// test the inverse of the max.
constexpr int64_t kMaxNegativeByteSizeDelta =
    -std::numeric_limits<int64_t>::max();
static_assert(kMaxNegativeByteSizeDelta == kMinByteSizeDelta + 1);

constexpr ByteSizeDelta kByteSizeDeltaNegativeMax(kMaxNegativeByteSizeDelta);

// Helper to generate a fixed number at runtime.
template <typename T>
T RunTimeNum(T num) {
  // RandInt always returns 0, but the compiler doesn't know that!
  return num + RandInt(0, 0);
}

TEST(ByteSizeTest, ConstructionDefault) {
  constexpr ByteSize bytes;

  EXPECT_EQ(bytes.InBytes(), 0);
  EXPECT_EQ(bytes.InKiB(), 0);
  EXPECT_EQ(bytes.InMiB(), 0);
  EXPECT_EQ(bytes.InGiB(), 0);
  EXPECT_EQ(bytes.InTiB(), 0);
  EXPECT_EQ(bytes.InPiB(), 0);
  EXPECT_EQ(bytes.InEiB(), 0);

  EXPECT_EQ(bytes.InBytesF(), 0.0);
  EXPECT_EQ(bytes.InKiBF(), 0.0);
  EXPECT_EQ(bytes.InMiBF(), 0.0);
  EXPECT_EQ(bytes.InGiBF(), 0.0);
  EXPECT_EQ(bytes.InTiBF(), 0.0);
  EXPECT_EQ(bytes.InPiBF(), 0.0);
  EXPECT_EQ(bytes.InEiBF(), 0.0);
}

TEST(ByteSizeDeathTest, ConstructionFromUnsigned) {
  constexpr unsigned kNumBytes = 1024 * 1024 * 1024;
  constexpr ByteSize bytes(kNumBytes);

  EXPECT_EQ(bytes.InBytes(), 1024 * 1024 * 1024);
  EXPECT_EQ(bytes.InKiB(), 1024 * 1024);
  EXPECT_EQ(bytes.InMiB(), 1024);
  EXPECT_EQ(bytes.InGiB(), 1);
  EXPECT_EQ(bytes.InTiB(), 0);
  EXPECT_EQ(bytes.InPiB(), 0);
  EXPECT_EQ(bytes.InEiB(), 0);

  EXPECT_DOUBLE_EQ(bytes.InBytesF(), 1024.0 * 1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(bytes.InKiBF(), 1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(bytes.InMiBF(), 1024.0);
  EXPECT_DOUBLE_EQ(bytes.InGiBF(), 1.0);
  EXPECT_DOUBLE_EQ(bytes.InTiBF(), 1.0 / 1024.0);
  EXPECT_DOUBLE_EQ(bytes.InPiBF(), 1.0 / (1024.0 * 1024.0));
  EXPECT_DOUBLE_EQ(bytes.InEiBF(), 1.0 / (1024.0 * 1024.0 * 1024.0));

  // Boundary conditions.
  EXPECT_EQ(ByteSize(0u), ByteSize());
  EXPECT_EQ(ByteSize(kMaxByteSize), ByteSize::Max());
  // TODO(crbug.com/448661443): Detect out-of-range unsigned constants at
  // compile time.
  BASE_EXPECT_DEATH(ByteSize(kMaxByteSize + 1), "");

  // Make sure non-const expressions are accepted.
  EXPECT_EQ(ByteSize(RunTimeNum(kNumBytes)), bytes);
}

TEST(ByteSizeTest, ConstructionFromSigned) {
  // Only allowed from constant expressions.
  EXPECT_EQ(ByteSize(100), ByteSize(100u));
  EXPECT_EQ(ByteSize(0), ByteSize());
  EXPECT_EQ(ByteSize(kMaxByteSizeDelta), ByteSize::Max());

  // Non-const expressions and negative constants are tested in
  // byte_size_nocompile.nc.
}

TEST(ByteSizeTest, ConstructionDelta) {
  for (auto delta : {ByteSizeDelta(), ByteSizeDelta(7), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    ByteSize bytes = ByteSize::FromByteSizeDelta(delta);
    ByteSize bytes2 = delta.AsByteSize();
    EXPECT_EQ(bytes.InBytes(), delta.InBytes());
    EXPECT_EQ(bytes, bytes2);
    EXPECT_EQ(bytes.AsByteSizeDelta(), delta);
    EXPECT_EQ(bytes2.AsByteSizeDelta(), delta);
  }

  // Make sure non-const expressions are accepted.
  ByteSizeDelta delta(RunTimeNum(3));
  EXPECT_EQ(ByteSize::FromByteSizeDelta(delta), ByteSize(3u));
  EXPECT_EQ(delta.AsByteSize(), ByteSize(3u));
}

TEST(ByteSizeDeathTest, ConstructionDeltaOutOfRange) {
  BASE_EXPECT_DEATH(ByteSize::FromByteSizeDelta(ByteSizeDelta(-1)), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(ByteSizeDelta(-1).AsByteSize(), "");
    BASE_EXPECT_DEATH(ByteSize::FromByteSizeDelta(ByteSizeDelta::Min()), "");
    BASE_EXPECT_DEATH(ByteSizeDelta::Min().AsByteSize(), "");
  }
}

TEST(ByteSizeTest, ConstructionMax) {
  EXPECT_EQ(ByteSize::Max().InBytes(), kMaxByteSize);
  EXPECT_DOUBLE_EQ(ByteSize::Max().InBytesF(),
                   static_cast<double>(kMaxByteSize));
}

TEST(ByteSizeTest, ConstructionOtherUnitIntegral) {
  EXPECT_EQ(KiBU(5u).InBytes(), 5u * 1024);
  EXPECT_EQ(KiBU(5u).InKiB(), 5u);

  EXPECT_EQ(MiBU(2u).InBytes(), 2u * 1024 * 1024);
  EXPECT_EQ(MiBU(2u).InMiB(), 2u);

  EXPECT_EQ(GiBU(12u).InBytes(), 12ull * 1024 * 1024 * 1024);
  EXPECT_EQ(GiBU(12u).InGiB(), 12u);

  EXPECT_EQ(TiBU(39u).InBytes(), 39ull * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(TiBU(39u).InTiB(), 39u);

  EXPECT_EQ(PiBU(7u).InBytes(), 7ull * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(PiBU(7u).InPiB(), 7u);

  EXPECT_EQ(EiBU(5u).InBytes(), 5ull * 1024 * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(EiBU(5u).InEiB(), 5u);

  // Make sure non-const unsigned expressions are accepted.
  EXPECT_EQ(KiBU(RunTimeNum(1u)).InKiB(), 1);
  EXPECT_EQ(MiBU(RunTimeNum(1u)).InMiB(), 1);
  EXPECT_EQ(GiBU(RunTimeNum(1u)).InGiB(), 1);
  EXPECT_EQ(TiBU(RunTimeNum(1u)).InTiB(), 1);
  EXPECT_EQ(PiBU(RunTimeNum(1u)).InPiB(), 1);
  EXPECT_EQ(EiBU(RunTimeNum(1u)).InEiB(), 1);

  // Make sure constant positive signed ints are accepted.
  // Non-const signed ints are tested in byte_size_nocompile.nc.
  EXPECT_EQ(KiBU(1).InKiB(), 1);
  EXPECT_EQ(MiBU(1).InMiB(), 1);
  EXPECT_EQ(GiBU(1).InGiB(), 1);
  EXPECT_EQ(TiBU(1).InTiB(), 1);
  EXPECT_EQ(PiBU(1).InPiB(), 1);
  EXPECT_EQ(EiBU(1).InEiB(), 1);
}

TEST(ByteSizeTest, ConstructionOtherUnitFloat) {
  EXPECT_EQ(KiBU(5.5).InBytes(), 5632);
  EXPECT_EQ(KiBU(5.5).InKiB(), 5);
  EXPECT_DOUBLE_EQ(KiBU(5.5).InBytesF(), 5632.0);
  EXPECT_DOUBLE_EQ(KiBU(5.5).InKiBF(), 5.5);

  // Round down from 2411724.8 to integral number of bytes.
  EXPECT_EQ(MiBU(2.3).InBytes(), 2411724);
  EXPECT_EQ(MiBU(2.3).InMiB(), 2);
  EXPECT_DOUBLE_EQ(MiBU(2.3).InBytesF(), 2411724.0);
  EXPECT_NEAR(MiBU(2.3).InMiBF(), 2.299, 0.001);  // Rounded bytes in MiB.

  // Round down from 13754632765.4 to integral number of bytes.
  EXPECT_EQ(GiBU(12.81).InBytes(), 13754632765);
  EXPECT_EQ(GiBU(12.81).InGiB(), 12);
  EXPECT_DOUBLE_EQ(GiBU(12.81).InBytesF(), 13754632765.0);
  EXPECT_NEAR(GiBU(12.81).InGiBF(), 12.809, 0.001);  // Rounded bytes in GiB.

  // Round down from 43760562785484.8 to integral number of bytes.
  EXPECT_EQ(TiBU(39.8).InBytes(), 43760562785484);
  EXPECT_EQ(TiBU(39.8).InTiB(), 39);
  EXPECT_DOUBLE_EQ(TiBU(39.8).InBytesF(), 43760562785484.0);
  EXPECT_NEAR(TiBU(39.8).InTiBF(), 39.799, 0.001);  // Rounded bytes in TiB.

  // 7.09 PiB is an integral number of bytes.
  EXPECT_EQ(PiBU(7.09).InBytes(), 7982630339514204);
  EXPECT_EQ(PiBU(7.09).InPiB(), 7);
  EXPECT_DOUBLE_EQ(PiBU(7.09).InBytesF(), 7982630339514204.0);
  EXPECT_DOUBLE_EQ(PiBU(7.09).InPiBF(), 7.09);

  // 5.36 EiB is an integral number of bytes.
  EXPECT_EQ(EiBU(5.36).InBytes(), 6179659264692700160);
  EXPECT_EQ(EiBU(5.36).InEiB(), 5);
  EXPECT_DOUBLE_EQ(EiBU(5.36).InBytesF(), 6179659264692700160.0);
  EXPECT_DOUBLE_EQ(EiBU(5.36).InEiBF(), 5.36);

  // Make sure non-const expressions are accepted.
  EXPECT_DOUBLE_EQ(KiBU(RunTimeNum(1.5)).InKiBF(), 1.5);
  EXPECT_DOUBLE_EQ(MiBU(RunTimeNum(1.5)).InMiBF(), 1.5);
  EXPECT_DOUBLE_EQ(GiBU(RunTimeNum(1.5)).InGiBF(), 1.5);
  EXPECT_DOUBLE_EQ(TiBU(RunTimeNum(1.5)).InTiBF(), 1.5);
  EXPECT_DOUBLE_EQ(PiBU(RunTimeNum(1.5)).InPiBF(), 1.5);
  EXPECT_DOUBLE_EQ(EiBU(RunTimeNum(1.5)).InEiBF(), 1.5);
}

TEST(ByteSizeTest, ConstructionOtherUnitConversionOrder) {
  // The max uint32_t held in a wider type so multiplication doesn't overflow.
  constexpr uint64_t kMaxU32 = std::numeric_limits<uint32_t>::max();

  // 32-bit numbers that will overflow if multiplied as CheckedNumeric<uint32_t>
  // but not if multiplied as CheckedNumeric<uint64_t>. This verifies that the
  // implementations of the templates avoid a subtle conversion order bug. Note
  // that dividing kMaxU32 by 1 TiB or higher rounds down to 1, which isn't
  // useful to test.
  constexpr uint32_t kLargeKiB = kMaxU32 / 1024 + 1;
  constexpr uint32_t kLargeMiB = kMaxU32 / 1024 / 1024 + 1;
  constexpr uint32_t kLargeGiB = kMaxU32 / 1024 / 1024 / 1024 + 1;

  // The expected results of converting the large numbers above to bytes.
  constexpr uint64_t kExpectedKiB = static_cast<uint64_t>(kLargeKiB) * 1024;
  constexpr uint64_t kExpectedMiB =
      static_cast<uint64_t>(kLargeMiB) * 1024 * 1024;
  constexpr uint64_t kExpectedGiB =
      static_cast<uint64_t>(kLargeGiB) * 1024 * 1024 * 1024;

  static_assert(kExpectedKiB > kMaxU32);
  static_assert(kExpectedMiB > kMaxU32);
  static_assert(kExpectedGiB > kMaxU32);

  // Make sure both float and int conversions work.
  EXPECT_EQ(KiBU(kLargeKiB).InBytes(), kExpectedKiB);
  EXPECT_EQ(KiBU(1.0 * kLargeKiB).InBytes(), kExpectedKiB);
  EXPECT_DOUBLE_EQ(KiBU(1.0 * kLargeKiB).InBytesF(), 1.0 * kExpectedKiB);

  EXPECT_EQ(MiBU(kLargeMiB).InBytes(), kExpectedMiB);
  EXPECT_EQ(MiBU(1.0 * kLargeMiB).InBytes(), kExpectedMiB);
  EXPECT_DOUBLE_EQ(MiBU(1.0 * kLargeMiB).InBytesF(), 1.0 * kExpectedMiB);

  EXPECT_EQ(GiBU(kLargeGiB).InBytes(), kExpectedGiB);
  EXPECT_EQ(GiBU(1.0 * kLargeGiB).InBytes(), kExpectedGiB);
  EXPECT_DOUBLE_EQ(GiBU(1.0 * kLargeGiB).InBytesF(), 1.0 * kExpectedGiB);
}

TEST(ByteSizeDeathTest, ConstructionOtherUnitOutOfRange) {
  // Negative and out-of-range const signed ints are tested in
  // byte_size_nocompile.nc.

  // Out-of-range unsigned ints.
  // TODO(crbug.com/448661443): Detect out-of-range unsigned constants at
  // compile time.
  BASE_EXPECT_DEATH(KiBU(kMaxByteSize), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(MiBU(kMaxByteSize), "");
    BASE_EXPECT_DEATH(GiBU(kMaxByteSize), "");
    BASE_EXPECT_DEATH(TiBU(kMaxByteSize), "");
    BASE_EXPECT_DEATH(PiBU(kMaxByteSize), "");
    BASE_EXPECT_DEATH(EiBU(kMaxByteSize), "");
  }

  // Negative and out-of-range floats.
  if (kRunAllDeathTests) {
    constexpr double kLargeFloat = static_cast<double>(kMaxByteSize);
    BASE_EXPECT_DEATH(KiBU(-1.0), "");
    BASE_EXPECT_DEATH(MiBU(-1.0), "");
    BASE_EXPECT_DEATH(GiBU(-1.0), "");
    BASE_EXPECT_DEATH(TiBU(-1.0), "");
    BASE_EXPECT_DEATH(PiBU(-1.0), "");
    BASE_EXPECT_DEATH(EiBU(-1.0), "");
    BASE_EXPECT_DEATH(KiBU(kLargeFloat), "");
    BASE_EXPECT_DEATH(MiBU(kLargeFloat), "");
    BASE_EXPECT_DEATH(GiBU(kLargeFloat), "");
    BASE_EXPECT_DEATH(TiBU(kLargeFloat), "");
    BASE_EXPECT_DEATH(PiBU(kLargeFloat), "");
    BASE_EXPECT_DEATH(EiBU(kLargeFloat), "");
  }
}

TEST(ByteSizeTest, IsZero) {
  EXPECT_TRUE(ByteSize(0).is_zero());
  EXPECT_FALSE(ByteSize(2).is_zero());
  EXPECT_FALSE(ByteSize::Max().is_zero());
}

TEST(ByteSizeTest, IsMax) {
  EXPECT_FALSE(ByteSize(0).is_max());
  EXPECT_FALSE(ByteSize(2).is_max());
  EXPECT_TRUE(ByteSize::Max().is_max());
}

TEST(ByteSizeTest, InFloating) {
  constexpr ByteSize bytes(3435973836);
  EXPECT_DOUBLE_EQ(bytes.InBytesF(), 3435973836.0);
  EXPECT_DOUBLE_EQ(bytes.InKiBF(), 3355443.19921875);
  EXPECT_DOUBLE_EQ(bytes.InMiBF(), 3276.7999992370605);
  EXPECT_DOUBLE_EQ(bytes.InGiBF(), 3.1999999992549419);
  constexpr ByteSize more_bytes(3435973836343597383);
  EXPECT_DOUBLE_EQ(more_bytes.InTiBF(), 3124999.9995849044);
  EXPECT_DOUBLE_EQ(more_bytes.InPiBF(), 3051.7578120946332);
  EXPECT_DOUBLE_EQ(more_bytes.InEiBF(), 2.9802322383736652);
}

// ByteSize + ByteSize -> ByteSize, range [0...max]

TEST(ByteSizeTest, AddByteSizeZero) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes + ByteSize(), bytes);
    ByteSize bytes2 = bytes;
    bytes2 += ByteSize();
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeDeathTest, AddByteSizePositive) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes + ByteSize(42), ByteSize(42));
  min_bytes += ByteSize(23);
  EXPECT_EQ(min_bytes, ByteSize(23));

  ByteSize bytes(42);
  EXPECT_EQ(bytes + ByteSize(5), ByteSize(47));
  bytes += ByteSize(6);
  EXPECT_EQ(bytes, ByteSize(48));

  ByteSize max_bytes = ByteSize::Max();
  BASE_EXPECT_DEATH(max_bytes + ByteSize(1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(max_bytes += ByteSize(1), "");
  }
}

TEST(ByteSizeDeathTest, AddByteSizeMax) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes + ByteSize::Max(), ByteSize::Max());
  min_bytes += ByteSize::Max();
  EXPECT_EQ(min_bytes, ByteSize::Max());

  if (kRunAllDeathTests) {
    ByteSize bytes(1);
    BASE_EXPECT_DEATH(bytes + ByteSize::Max(), "");
    BASE_EXPECT_DEATH(bytes += ByteSize::Max(), "");

    // Make sure max + max doesn't wrap around.
    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes + ByteSize::Max(), "");
    BASE_EXPECT_DEATH(max_bytes += ByteSize::Max(), "");
  }
}

// ByteSize + ByteSizeDelta -> ByteSize, range [0...max]

TEST(ByteSizeDeathTest, AddByteSizeDeltaMin) {
  // Add min (equivalent to subtracting max+1).
  // This is always out of range (will be < 0).
  if (kRunAllDeathTests) {
    for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
      SCOPED_TRACE(bytes);
      BASE_EXPECT_DEATH(bytes + ByteSizeDelta::Min(), "");
      BASE_EXPECT_DEATH(ByteSizeDelta::Min() + bytes, "");
      BASE_EXPECT_DEATH(bytes += ByteSizeDelta::Min(), "");
    }
  } else {
    BASE_EXPECT_DEATH(ByteSize::Max() + ByteSizeDelta::Min(), "");
  }
}

TEST(ByteSizeDeathTest, AddByteSizeDeltaNegativeMax) {
  // Add -max (equivalent to subtracting max).
  if (kRunAllDeathTests) {
    ByteSize min_bytes;
    BASE_EXPECT_DEATH(min_bytes + kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(kByteSizeDeltaNegativeMax + min_bytes, "");
    BASE_EXPECT_DEATH(min_bytes += kByteSizeDeltaNegativeMax, "");

    ByteSize bytes(kMaxByteSize - 1);
    BASE_EXPECT_DEATH(bytes + kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(kByteSizeDeltaNegativeMax + bytes, "");
    BASE_EXPECT_DEATH(bytes += kByteSizeDeltaNegativeMax, "");
  }

  ByteSize max_bytes = ByteSize::Max();
  EXPECT_EQ(max_bytes + kByteSizeDeltaNegativeMax, ByteSize());
  EXPECT_EQ(kByteSizeDeltaNegativeMax + max_bytes, ByteSize());
  max_bytes += kByteSizeDeltaNegativeMax;
  EXPECT_EQ(max_bytes, ByteSize());
}

TEST(ByteSizeDeathTest, AddByteSizeDeltaNegative) {
  ByteSize min_bytes;
  BASE_EXPECT_DEATH(min_bytes + ByteSizeDelta(-1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(ByteSizeDelta(-1) + min_bytes, "");
    BASE_EXPECT_DEATH(min_bytes += ByteSizeDelta(-1), "");
  }

  ByteSize bytes(42);
  EXPECT_EQ(bytes + ByteSizeDelta(-10), ByteSize(32));
  EXPECT_EQ(ByteSizeDelta(-10) + bytes, ByteSize(32));
  EXPECT_EQ(bytes + ByteSizeDelta(-42), ByteSize());
  EXPECT_EQ(ByteSizeDelta(-42) + bytes, ByteSize());
  bytes += ByteSizeDelta(-2);
  EXPECT_EQ(bytes, ByteSize(40));
  bytes += ByteSizeDelta(-40);
  EXPECT_EQ(bytes, ByteSize());
}

TEST(ByteSizeTest, AddByteSizeDeltaZero) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes + ByteSizeDelta(), bytes);
    EXPECT_EQ(ByteSizeDelta() + bytes, bytes);
    ByteSize bytes2 = bytes;
    bytes2 += ByteSizeDelta();
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeDeathTest, AddByteSizeDeltaPositive) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes + ByteSizeDelta(100), ByteSize(100));
  EXPECT_EQ(ByteSizeDelta(100) + min_bytes, ByteSize(100));
  min_bytes += ByteSizeDelta(50);
  EXPECT_EQ(min_bytes, ByteSize(50));

  ByteSize bytes(42);
  EXPECT_EQ(bytes + ByteSizeDelta(10), ByteSize(52));
  EXPECT_EQ(ByteSizeDelta(10) + bytes, ByteSize(52));
  bytes += ByteSizeDelta(2);
  EXPECT_EQ(bytes, ByteSize(44));

  ByteSize large_bytes(kMaxByteSize - 1);
  EXPECT_EQ(large_bytes + ByteSizeDelta(1), ByteSize::Max());
  EXPECT_EQ(ByteSizeDelta(1) + large_bytes, ByteSize::Max());
  large_bytes += ByteSizeDelta(1);
  EXPECT_EQ(large_bytes, ByteSize::Max());

  ByteSize max_bytes = ByteSize::Max();
  BASE_EXPECT_DEATH(max_bytes + ByteSizeDelta(1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(ByteSizeDelta(1) + max_bytes, "");
    BASE_EXPECT_DEATH(max_bytes += ByteSizeDelta(1), "");
  }
}

TEST(ByteSizeDeathTest, AddByteSizeDeltaMax) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes + ByteSizeDelta::Max(), ByteSize::Max());
  EXPECT_EQ(ByteSizeDelta::Max() + min_bytes, ByteSize::Max());
  min_bytes += ByteSizeDelta::Max();
  EXPECT_EQ(min_bytes, ByteSize::Max());

  if (kRunAllDeathTests) {
    ByteSize bytes(1);
    BASE_EXPECT_DEATH(bytes + ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(ByteSizeDelta::Max() + bytes, "");
    BASE_EXPECT_DEATH(bytes += ByteSizeDelta::Max(), "");

    // Make sure max + max doesn't wrap around.
    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes + ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(ByteSizeDelta::Max() + max_bytes, "");
    BASE_EXPECT_DEATH(max_bytes += ByteSizeDelta::Max(), "");
  }
}

// ByteSize - ByteSize -> ByteSizeDelta, range [min...max]

TEST(ByteSizeTest, SubtractByteSizeZero) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes - ByteSize(), bytes.AsByteSizeDelta());
    ByteSize bytes2 = bytes;
    bytes2 -= ByteSize();
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeTest, SubtractByteSizePositive) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes - ByteSize(100), ByteSizeDelta(-100));
  BASE_EXPECT_DEATH(min_bytes -= ByteSize(1), "");

  ByteSize bytes(42);
  EXPECT_EQ(bytes - ByteSize(10), ByteSizeDelta(32));
  EXPECT_EQ(bytes - ByteSize(50), ByteSizeDelta(-8));
  EXPECT_EQ(bytes - ByteSize(42), ByteSizeDelta());
  bytes -= ByteSize(12);
  EXPECT_EQ(bytes, ByteSize(30));
  bytes -= ByteSize(30);
  EXPECT_EQ(bytes, ByteSize());
}

TEST(ByteSizeDeathTest, SubtractByteSizeMax) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes - ByteSize::Max(), kByteSizeDeltaNegativeMax);
  BASE_EXPECT_DEATH(min_bytes -= ByteSize::Max(), "");

  ByteSize max_bytes = ByteSize::Max();
  EXPECT_EQ(max_bytes - ByteSize::Max(), ByteSizeDelta());
  max_bytes -= ByteSize::Max();
  EXPECT_EQ(max_bytes, ByteSize());
}

// ByteSize - ByteSizeDelta -> ByteSize, range [0...max]

TEST(ByteSizeDeathTest, SubtractByteSizeDeltaMin) {
  // Subtract min (equivalent to adding max+1).
  // This is always out of range (will be > ByteSize::Max()).
  if (kRunAllDeathTests) {
    for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
      SCOPED_TRACE(bytes);
      BASE_EXPECT_DEATH(bytes - ByteSizeDelta::Min(), "");
      BASE_EXPECT_DEATH(bytes -= ByteSizeDelta::Min(), "");
    }
  } else {
    BASE_EXPECT_DEATH(ByteSize() - ByteSizeDelta::Min(), "");
  }
}

TEST(ByteSizeDeathTest, SubtractByteSizeDeltaNegativeMax) {
  // Subtract -max (equivalent to adding max).
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes - kByteSizeDeltaNegativeMax, ByteSize::Max());
  min_bytes -= kByteSizeDeltaNegativeMax;
  EXPECT_EQ(min_bytes, ByteSize::Max());

  if (kRunAllDeathTests) {
    ByteSize bytes(1);
    BASE_EXPECT_DEATH(bytes - kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(bytes -= kByteSizeDeltaNegativeMax, "");

    // Make sure max + max doesn't wrap around.
    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes - kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(max_bytes -= kByteSizeDeltaNegativeMax, "");
  }
}

TEST(ByteSizeDeathTest, SubtractByteSizeDeltaNegative) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes - ByteSizeDelta(-10), ByteSize(10));
  min_bytes -= ByteSizeDelta(-100);
  EXPECT_EQ(min_bytes, ByteSize(100));

  ByteSize bytes(42);
  EXPECT_EQ(bytes - ByteSizeDelta(-10), ByteSize(52));
  bytes -= ByteSizeDelta(-40);
  EXPECT_EQ(bytes, ByteSize(82));

  ByteSize max_bytes = ByteSize::Max();
  BASE_EXPECT_DEATH(max_bytes - ByteSizeDelta(-1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(max_bytes -= ByteSizeDelta(-1), "");
  }
}

TEST(ByteSizeTest, SubtractByteSizeDeltaZero) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes - ByteSizeDelta(), bytes);
    ByteSize bytes2 = bytes;
    bytes2 -= ByteSizeDelta();
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeDeathTest, SubtractByteSizeDeltaPositive) {
  ByteSize min_bytes;
  BASE_EXPECT_DEATH(min_bytes - ByteSizeDelta(1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(min_bytes -= ByteSizeDelta(1), "");
  }

  ByteSize bytes(42);
  EXPECT_EQ(bytes - ByteSizeDelta(32), ByteSize(10));
  EXPECT_EQ(bytes - ByteSizeDelta(42), ByteSize());
  bytes -= ByteSizeDelta(32);
  EXPECT_EQ(bytes, ByteSize(10));
  bytes -= ByteSizeDelta(10);
  EXPECT_EQ(bytes, ByteSize());
}

TEST(ByteSizeDeathTest, SubtractByteSizeDeltaMax) {
  if (kRunAllDeathTests) {
    // Make sure min - max doesn't wrap around.
    ByteSize min_bytes;
    BASE_EXPECT_DEATH(min_bytes - ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(min_bytes -= ByteSizeDelta::Max(), "");

    ByteSize bytes(kMaxByteSize - 1);
    BASE_EXPECT_DEATH(bytes - ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(bytes -= ByteSizeDelta::Max(), "");
  }

  ByteSize max_bytes = ByteSize::Max();
  EXPECT_EQ(max_bytes - ByteSizeDelta::Max(), ByteSize());
  max_bytes -= ByteSizeDelta::Max();
  EXPECT_EQ(max_bytes, ByteSize());
}

TEST(ByteSizeTest, MultiplyByZero) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes * 0, ByteSize());
    EXPECT_EQ(0 * bytes, ByteSize());
    bytes *= 0;
    EXPECT_EQ(bytes, ByteSize());
  }
}

TEST(ByteSizeTest, MultiplyIdentity) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes * 1, bytes);
    EXPECT_EQ(1 * bytes, bytes);
    ByteSize bytes2 = bytes;
    bytes2 *= 1;
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeDeathTest, MultiplyInvert) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes * -1, ByteSize());
  EXPECT_EQ(-1 * min_bytes, ByteSize());
  min_bytes *= -1;
  EXPECT_EQ(min_bytes, ByteSize());

  ByteSize bytes(42);
  BASE_EXPECT_DEATH(bytes * -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(-1 * bytes, "");
    BASE_EXPECT_DEATH(bytes *= -1, "");
  }

  ByteSize max_bytes = ByteSize::Max();
  BASE_EXPECT_DEATH(max_bytes * -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(-1 * max_bytes, "");
    BASE_EXPECT_DEATH(max_bytes *= -1, "");
  }
}

TEST(ByteSizeDeathTest, MultiplyPositive) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes * 2, ByteSize());
  EXPECT_EQ(2 * min_bytes, ByteSize());
  min_bytes *= 2;
  EXPECT_EQ(min_bytes, ByteSize());

  ByteSize bytes(42);
  EXPECT_EQ(bytes * 2, ByteSize(84));
  EXPECT_EQ(2 * bytes, ByteSize(84));
  bytes *= 2;
  EXPECT_EQ(bytes, ByteSize(84));

  if (kRunAllDeathTests) {
    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes * 2, "");
    BASE_EXPECT_DEATH(2 * max_bytes, "");
    BASE_EXPECT_DEATH(max_bytes *= 2, "");
  }
}

TEST(ByteSizeDeathTest, MultiplyNegative) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes * -2, ByteSize());
  EXPECT_EQ(-2 * min_bytes, ByteSize());
  min_bytes *= -2;
  EXPECT_EQ(min_bytes, ByteSize());

  if (kRunAllDeathTests) {
    ByteSize bytes(42);
    BASE_EXPECT_DEATH(bytes * -2, "");
    BASE_EXPECT_DEATH(-2 * bytes, "");
    BASE_EXPECT_DEATH(bytes *= -2, "");

    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes * -2, "");
    BASE_EXPECT_DEATH(-2 * max_bytes, "");
    BASE_EXPECT_DEATH(max_bytes *= -2, "");
  }
}

TEST(ByteSizeTest, DivideIdentity) {
  for (auto bytes : {ByteSize(), ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes / 1, bytes);
    ByteSize bytes2 = bytes;
    bytes2 /= 1;
    EXPECT_EQ(bytes2, bytes);
  }
}

TEST(ByteSizeTest, DivideBySelf) {
  for (auto bytes : {ByteSize(42), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    EXPECT_EQ(bytes / bytes.InBytes(), ByteSize(1));
    bytes /= bytes.InBytes();
    EXPECT_EQ(bytes, ByteSize(1));
  }
}

TEST(ByteSizeDeathTest, DivideInvert) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes / -1, ByteSize());
  min_bytes /= -1;
  EXPECT_EQ(min_bytes, ByteSize());

  ByteSize bytes(42);
  BASE_EXPECT_DEATH(bytes / -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(bytes /= -1, "");
  }

  ByteSize max_bytes = ByteSize::Max();
  BASE_EXPECT_DEATH(max_bytes / -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(max_bytes /= -1, "");
  }
}

TEST(ByteSizeTest, DividePositive) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes / 2, ByteSize());
  min_bytes /= 2;
  EXPECT_EQ(min_bytes, ByteSize());

  ByteSize bytes(42);
  EXPECT_EQ(bytes / 2, ByteSize(21));
  bytes /= 2;
  EXPECT_EQ(bytes, ByteSize(21));

  ByteSize max_bytes = ByteSize::Max();
  EXPECT_EQ(max_bytes / 2, ByteSize(kMaxByteSize / 2));
  max_bytes /= 2;
  EXPECT_EQ(max_bytes, ByteSize(kMaxByteSize / 2));
}

TEST(ByteSizeDeathTest, DivideNegative) {
  ByteSize min_bytes;
  EXPECT_EQ(min_bytes / -2, ByteSize());
  min_bytes /= -2;
  EXPECT_EQ(min_bytes, ByteSize());

  if (kRunAllDeathTests) {
    ByteSize bytes(42);
    BASE_EXPECT_DEATH(bytes / -2, "");
    BASE_EXPECT_DEATH(bytes /= -2, "");

    ByteSize max_bytes = ByteSize::Max();
    BASE_EXPECT_DEATH(max_bytes / -2, "");
    BASE_EXPECT_DEATH(max_bytes /= -2, "");
  }
}

TEST(ByteSizeTest, Comparison) {
  constexpr ByteSize a(1);
  constexpr ByteSize b(2);
  constexpr ByteSize c(2);

  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
  EXPECT_FALSE(b < c);

  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(b <= a);
  EXPECT_TRUE(b <= c);

  EXPECT_FALSE(a > b);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b > c);

  EXPECT_FALSE(a >= b);
  EXPECT_TRUE(b >= a);
  EXPECT_TRUE(b >= c);

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(b == c);

  EXPECT_TRUE(a != b);
  EXPECT_FALSE(b != c);

  // Boundary conditions.
  for (ByteSize other : {a, ByteSize::Max()}) {
    SCOPED_TRACE(other);

    EXPECT_TRUE(ByteSize() < other);
    EXPECT_TRUE(ByteSize() <= other);
    EXPECT_FALSE(ByteSize() > other);
    EXPECT_FALSE(ByteSize() >= other);
    EXPECT_FALSE(ByteSize() == other);
    EXPECT_TRUE(ByteSize() != other);

    EXPECT_FALSE(other < ByteSize());
    EXPECT_FALSE(other <= ByteSize());
    EXPECT_TRUE(other > ByteSize());
    EXPECT_TRUE(other >= ByteSize());
    EXPECT_FALSE(other == ByteSize());
    EXPECT_TRUE(other != ByteSize());
  }
  for (ByteSize other : {ByteSize(), a}) {
    SCOPED_TRACE(other);

    EXPECT_FALSE(ByteSize::Max() < other);
    EXPECT_FALSE(ByteSize::Max() <= other);
    EXPECT_TRUE(ByteSize::Max() > other);
    EXPECT_TRUE(ByteSize::Max() >= other);
    EXPECT_FALSE(ByteSize::Max() == other);
    EXPECT_TRUE(ByteSize::Max() != other);

    EXPECT_TRUE(other < ByteSize::Max());
    EXPECT_TRUE(other <= ByteSize::Max());
    EXPECT_FALSE(other > ByteSize::Max());
    EXPECT_FALSE(other >= ByteSize::Max());
    EXPECT_FALSE(other == ByteSize::Max());
    EXPECT_TRUE(other != ByteSize::Max());
  }
}

TEST(ByteSizeTest, StreamOperator) {
  struct TestValue {
    uint64_t bytes;
    const char* expected;
  } kTestValues[] = {
      {0, "0B"},
      {1, "1B"},

      {1024 - 1, "1023B"},
      {1024, "1KiB"},
      {1024 + 1, "1025B (1.001KiB)"},

      {1024 * 1024 - 1, "1048575B (1023.999KiB)"},
      {1024 * 1024, "1MiB"},
      {1024 * 1024 + 1'000, "1049576B (1.001MiB)"},

      {1024LL * 1024 * 1024 - 1'000, "1073740824B (1023.999MiB)"},
      {1024LL * 1024 * 1024, "1GiB"},
      {1024LL * 1024 * 1024 + 1'000'000, "1074741824B (1.001GiB)"},

      {1024LL * 1024 * 1024 * 1024 - 1'000'000, "1099510627776B (1023.999GiB)"},
      {1024LL * 1024 * 1024 * 1024, "1TiB"},
      {1024LL * 1024 * 1024 * 1024 + 1'000'000'000,
       "1100511627776B (1.001TiB)"},

      {1024LL * 1024 * 1024 * 1024 * 1024 - 1'000'000'000,
       "1125898906842624B (1023.999TiB)"},
      {1024LL * 1024 * 1024 * 1024 * 1024, "1PiB"},
      {1024LL * 1024 * 1024 * 1024 * 1024 + 1'000'000'000'000,
       "1126899906842624B (1.001PiB)"},

      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024 - 1'000'000'000'000,
       "1152920504606846976B (1023.999PiB)"},
      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024, "1EiB"},
      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024 + 1'000'000'000'000'000,
       "1153921504606846976B (1.001EiB)"},

      {ByteSize::Max().InBytes(), "9223372036854775807B (8.000EiB)"},
  };
  for (const auto& test_value : kTestValues) {
    std::stringstream ss;
    ss << ByteSize(test_value.bytes);
    EXPECT_EQ(test_value.expected, ss.str());
  }
}

TEST(ByteSizeDeltaTest, ConstructionDefault) {
  constexpr ByteSizeDelta delta;

  EXPECT_EQ(delta.InBytes(), 0);
  EXPECT_EQ(delta.InKiB(), 0);
  EXPECT_EQ(delta.InMiB(), 0);
  EXPECT_EQ(delta.InGiB(), 0);
  EXPECT_EQ(delta.InTiB(), 0);
  EXPECT_EQ(delta.InPiB(), 0);
  EXPECT_EQ(delta.InEiB(), 0);

  EXPECT_EQ(delta.InBytesF(), 0.0);
  EXPECT_EQ(delta.InKiBF(), 0.0);
  EXPECT_EQ(delta.InMiBF(), 0.0);
  EXPECT_EQ(delta.InGiBF(), 0.0);
  EXPECT_EQ(delta.InTiBF(), 0.0);
  EXPECT_EQ(delta.InPiBF(), 0.0);
  EXPECT_EQ(delta.InEiBF(), 0.0);
}

TEST(ByteSizeDeltaTest, ConstructionFromSigned) {
  constexpr int kPositiveBytes = 1024 * 1024 * 1024;
  constexpr ByteSizeDelta delta(kPositiveBytes);

  EXPECT_EQ(delta.InBytes(), 1024 * 1024 * 1024);
  EXPECT_EQ(delta.InKiB(), 1024 * 1024);
  EXPECT_EQ(delta.InMiB(), 1024);
  EXPECT_EQ(delta.InGiB(), 1);
  EXPECT_EQ(delta.InTiB(), 0);
  EXPECT_EQ(delta.InPiB(), 0);
  EXPECT_EQ(delta.InEiB(), 0);

  EXPECT_DOUBLE_EQ(delta.InBytesF(), 1024.0 * 1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(delta.InKiBF(), 1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(delta.InMiBF(), 1024.0);
  EXPECT_DOUBLE_EQ(delta.InGiBF(), 1.0);
  EXPECT_DOUBLE_EQ(delta.InTiBF(), 1.0 / 1024.0);
  EXPECT_DOUBLE_EQ(delta.InPiBF(), 1.0 / (1024.0 * 1024.0));
  EXPECT_DOUBLE_EQ(delta.InEiBF(), 1.0 / (1024.0 * 1024.0 * 1024.0));

  constexpr int kNegativeBytes = -1024 * 1024 * 1024;
  constexpr ByteSizeDelta negative(kNegativeBytes);

  EXPECT_EQ(negative.InBytes(), -1024 * 1024 * 1024);
  EXPECT_EQ(negative.InKiB(), -1024 * 1024);
  EXPECT_EQ(negative.InMiB(), -1024);
  EXPECT_EQ(negative.InGiB(), -1);
  EXPECT_EQ(negative.InTiB(), 0);
  EXPECT_EQ(negative.InPiB(), 0);
  EXPECT_EQ(negative.InEiB(), 0);

  EXPECT_DOUBLE_EQ(negative.InBytesF(), -1024.0 * 1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(negative.InKiBF(), -1024.0 * 1024.0);
  EXPECT_DOUBLE_EQ(negative.InMiBF(), -1024.);
  EXPECT_DOUBLE_EQ(negative.InGiBF(), -1.0);
  EXPECT_DOUBLE_EQ(negative.InTiBF(), -1.0 / 1024.0);
  EXPECT_DOUBLE_EQ(negative.InPiBF(), -1.0 / (1024.0 * 1024.0));
  EXPECT_DOUBLE_EQ(negative.InEiBF(), -1.0 / (1024.0 * 1024.0 * 1024.0));

  // Boundary conditions.
  EXPECT_EQ(ByteSizeDelta(kMinByteSizeDelta), ByteSizeDelta::Min());
  EXPECT_EQ(ByteSizeDelta(0), ByteSizeDelta());
  EXPECT_EQ(ByteSizeDelta(kMaxByteSizeDelta), ByteSizeDelta::Max());

  // Make sure non-const expressions are accepted.
  EXPECT_EQ(ByteSizeDelta(RunTimeNum(kPositiveBytes)), delta);
  EXPECT_EQ(ByteSizeDelta(RunTimeNum(kNegativeBytes)), negative);
}

TEST(ByteSizeDeltaDeathTest, ConstructionFromUnsigned) {
  // Only allowed from constant expressions.
  EXPECT_EQ(ByteSizeDelta(100u), ByteSizeDelta(100));
  EXPECT_EQ(ByteSizeDelta(0u), ByteSizeDelta());
  EXPECT_EQ(ByteSizeDelta(kMaxByteSize), ByteSizeDelta::Max());

  // Non-const expressions and out-of-range constants are tested in
  // byte_size_nocompile.nc.
}

TEST(ByteSizeDeltaTest, ConstructionByteSize) {
  for (auto bytes : {ByteSize(), ByteSize(7), ByteSize::Max()}) {
    SCOPED_TRACE(bytes);
    ByteSizeDelta delta = ByteSizeDelta::FromByteSize(bytes);
    ByteSizeDelta delta2 = bytes.AsByteSizeDelta();
    EXPECT_EQ(delta.InBytes(), bytes.InBytes());
    EXPECT_EQ(delta, delta2);
    EXPECT_EQ(delta.AsByteSize(), bytes);
    EXPECT_EQ(delta2.AsByteSize(), bytes);
  }

  // Make sure non-const expressions are accepted.
  ByteSize bytes(RunTimeNum(3u));
  EXPECT_EQ(ByteSizeDelta::FromByteSize(bytes), ByteSizeDelta(3));
  EXPECT_EQ(bytes.AsByteSizeDelta(), ByteSizeDelta(3));
}

TEST(ByteSizeDeltaTest, ConstructionMin) {
  EXPECT_EQ(ByteSizeDelta::Min().InBytes(), kMinByteSizeDelta);
  EXPECT_DOUBLE_EQ(ByteSizeDelta::Min().InBytesF(),
                   static_cast<double>(kMinByteSizeDelta));
}

TEST(ByteSizeDeltaTest, ConstructionMax) {
  EXPECT_EQ(ByteSizeDelta::Max().InBytes(), kMaxByteSizeDelta);
  EXPECT_DOUBLE_EQ(ByteSizeDelta::Max().InBytesF(),
                   static_cast<double>(kMaxByteSizeDelta));
}

TEST(ByteSizeDeltaTest, ConstructionOtherUnitIntegral) {
  EXPECT_EQ(KiBS(5).InBytes(), 5 * 1024);
  EXPECT_EQ(KiBS(5).InKiB(), 5);
  EXPECT_EQ(KiBS(-5).InBytes(), -5 * 1024);
  EXPECT_EQ(KiBS(-5).InKiB(), -5);

  EXPECT_EQ(MiBS(2).InBytes(), 2 * 1024 * 1024);
  EXPECT_EQ(MiBS(2).InMiB(), 2);
  EXPECT_EQ(MiBS(-2).InBytes(), -2 * 1024 * 1024);
  EXPECT_EQ(MiBS(-2).InMiB(), -2);

  EXPECT_EQ(GiBS(12).InBytes(), 12ll * 1024 * 1024 * 1024);
  EXPECT_EQ(GiBS(12).InGiB(), 12);
  EXPECT_EQ(GiBS(-12).InBytes(), -12ll * 1024 * 1024 * 1024);
  EXPECT_EQ(GiBS(-12).InGiB(), -12);

  EXPECT_EQ(TiBS(39).InBytes(), 39ll * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(TiBS(39).InTiB(), 39);
  EXPECT_EQ(TiBS(-39).InBytes(), -39ll * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(TiBS(-39).InTiB(), -39);

  EXPECT_EQ(PiBS(7).InBytes(), 7ll * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(PiBS(7).InPiB(), 7);
  EXPECT_EQ(PiBS(-7).InBytes(), -7ll * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(PiBS(-7).InPiB(), -7);

  EXPECT_EQ(EiBS(5).InBytes(), 5ll * 1024 * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(EiBS(5).InEiB(), 5);
  EXPECT_EQ(EiBS(-5).InBytes(), -5ll * 1024 * 1024 * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(EiBS(-5).InEiB(), -5);

  // Make sure non-const signed expressions are accepted.
  EXPECT_EQ(KiBS(RunTimeNum(1)).InKiB(), 1);
  EXPECT_EQ(MiBS(RunTimeNum(1)).InMiB(), 1);
  EXPECT_EQ(GiBS(RunTimeNum(1)).InGiB(), 1);
  EXPECT_EQ(TiBS(RunTimeNum(1)).InTiB(), 1);
  EXPECT_EQ(PiBS(RunTimeNum(1)).InPiB(), 1);
  EXPECT_EQ(EiBS(RunTimeNum(1)).InEiB(), 1);

  EXPECT_EQ(KiBS(RunTimeNum(-1)).InKiB(), -1);
  EXPECT_EQ(MiBS(RunTimeNum(-1)).InMiB(), -1);
  EXPECT_EQ(GiBS(RunTimeNum(-1)).InGiB(), -1);
  EXPECT_EQ(TiBS(RunTimeNum(-1)).InTiB(), -1);
  EXPECT_EQ(PiBS(RunTimeNum(-1)).InPiB(), -1);
  EXPECT_EQ(EiBS(RunTimeNum(-1)).InEiB(), -1);

  // Make sure constant unsigned ints are accepted.
  // Non-const unsigned ints are tested in byte_size_nocompile.nc.
  EXPECT_EQ(KiBS(1u).InKiB(), 1u);
  EXPECT_EQ(MiBS(1u).InMiB(), 1u);
  EXPECT_EQ(GiBS(1u).InGiB(), 1u);
  EXPECT_EQ(TiBS(1u).InTiB(), 1u);
  EXPECT_EQ(PiBS(1u).InPiB(), 1u);
  EXPECT_EQ(EiBS(1u).InEiB(), 1u);
}

TEST(ByteSizeDeltaTest, ConstructionOtherUnitFloat) {
  EXPECT_EQ(KiBS(5.5).InBytes(), 5632);
  EXPECT_EQ(KiBS(5.5).InKiB(), 5);
  EXPECT_DOUBLE_EQ(KiBS(5.5).InBytesF(), 5632.0);
  EXPECT_DOUBLE_EQ(KiBS(5.5).InKiBF(), 5.5);

  EXPECT_EQ(KiBS(-5.5).InBytes(), -5632);
  EXPECT_EQ(KiBS(-5.5).InKiB(), -5);
  EXPECT_DOUBLE_EQ(KiBS(-5.5).InBytesF(), -5632.0);
  EXPECT_DOUBLE_EQ(KiBS(-5.5).InKiBF(), -5.5);

  // Round +-2411724.8 toward 0 to get an integral number of bytes.
  EXPECT_EQ(MiBS(2.3).InBytes(), 2411724);
  EXPECT_EQ(MiBS(2.3).InMiB(), 2);
  EXPECT_DOUBLE_EQ(MiBS(2.3).InBytesF(), 2411724.0);
  EXPECT_NEAR(MiBS(2.3).InMiBF(), 2.299, 0.001);  // Rounded bytes in MiB.

  EXPECT_EQ(MiBS(-2.3).InBytes(), -2411724);
  EXPECT_EQ(MiBS(-2.3).InMiB(), -2);
  EXPECT_DOUBLE_EQ(MiBS(-2.3).InBytesF(), -2411724.0);
  EXPECT_NEAR(MiBS(-2.3).InMiBF(), -2.299, 0.001);  // Rounded bytes in MiB.

  // Round +-13754632765.4 toward 0 to get an integral number of bytes.
  EXPECT_EQ(GiBS(12.81).InBytes(), 13754632765);
  EXPECT_EQ(GiBS(12.81).InGiB(), 12);
  EXPECT_DOUBLE_EQ(GiBS(12.81).InBytesF(), 13754632765.0);
  EXPECT_NEAR(GiBS(12.81).InGiBF(), 12.809, 0.001);  // Rounded bytes in GiB.

  EXPECT_EQ(GiBS(-12.81).InBytes(), -13754632765);
  EXPECT_EQ(GiBS(-12.81).InGiB(), -12);
  EXPECT_DOUBLE_EQ(GiBS(-12.81).InBytesF(), -13754632765.0);
  EXPECT_NEAR(GiBS(-12.81).InGiBF(), -12.809, 0.001);  // Rounded bytes in GiB.

  // Round +-43760562785484.8 toward 0 to get an integral number of bytes.
  EXPECT_EQ(TiBS(39.8).InBytes(), 43760562785484);
  EXPECT_EQ(TiBS(39.8).InTiB(), 39);
  EXPECT_DOUBLE_EQ(TiBS(39.8).InBytesF(), 43760562785484.0);
  EXPECT_NEAR(TiBS(39.8).InTiBF(), 39.799, 0.001);  // Rounded bytes in TiB.

  EXPECT_EQ(TiBS(-39.8).InBytes(), -43760562785484);
  EXPECT_EQ(TiBS(-39.8).InTiB(), -39);
  EXPECT_DOUBLE_EQ(TiBS(-39.8).InBytesF(), -43760562785484.0);
  EXPECT_NEAR(TiBS(-39.8).InTiBF(), -39.799, 0.001);  // Rounded bytes in TiB.

  // +-7.09 PiB is an integral number of bytes.
  EXPECT_EQ(PiBS(7.09).InBytes(), 7982630339514204);
  EXPECT_EQ(PiBS(7.09).InPiB(), 7);
  EXPECT_DOUBLE_EQ(PiBS(7.09).InBytesF(), 7982630339514204.0);
  EXPECT_DOUBLE_EQ(PiBS(7.09).InPiBF(), 7.09);

  EXPECT_EQ(PiBS(-7.09).InBytes(), -7982630339514204);
  EXPECT_EQ(PiBS(-7.09).InPiB(), -7);
  EXPECT_DOUBLE_EQ(PiBS(-7.09).InBytesF(), -7982630339514204.0);
  EXPECT_DOUBLE_EQ(PiBS(-7.09).InPiBF(), -7.09);

  // +-5.36 EiB is an integral number of bytes.
  EXPECT_EQ(EiBS(5.36).InBytes(), 6179659264692700160);
  EXPECT_EQ(EiBS(5.36).InEiB(), 5);
  EXPECT_DOUBLE_EQ(EiBS(5.36).InBytesF(), 6179659264692700160.0);
  EXPECT_DOUBLE_EQ(EiBS(5.36).InEiBF(), 5.36);

  EXPECT_EQ(EiBS(-5.36).InBytes(), -6179659264692700160);
  EXPECT_EQ(EiBS(-5.36).InEiB(), -5);
  EXPECT_DOUBLE_EQ(EiBS(-5.36).InBytesF(), -6179659264692700160.0);
  EXPECT_DOUBLE_EQ(EiBS(-5.36).InEiBF(), -5.36);

  // Make sure non-const expressions are accepted.
  EXPECT_DOUBLE_EQ(KiBS(RunTimeNum(1.5)).InKiBF(), 1.5);
  EXPECT_DOUBLE_EQ(MiBS(RunTimeNum(1.5)).InMiBF(), 1.5);
  EXPECT_DOUBLE_EQ(GiBS(RunTimeNum(1.5)).InGiBF(), 1.5);
  EXPECT_DOUBLE_EQ(TiBS(RunTimeNum(1.5)).InTiBF(), 1.5);
  EXPECT_DOUBLE_EQ(PiBS(RunTimeNum(1.5)).InPiBF(), 1.5);
  EXPECT_DOUBLE_EQ(EiBS(RunTimeNum(1.5)).InEiBF(), 1.5);

  EXPECT_DOUBLE_EQ(KiBS(RunTimeNum(-1.5)).InKiBF(), -1.5);
  EXPECT_DOUBLE_EQ(MiBS(RunTimeNum(-1.5)).InMiBF(), -1.5);
  EXPECT_DOUBLE_EQ(GiBS(RunTimeNum(-1.5)).InGiBF(), -1.5);
  EXPECT_DOUBLE_EQ(TiBS(RunTimeNum(-1.5)).InTiBF(), -1.5);
  EXPECT_DOUBLE_EQ(PiBS(RunTimeNum(-1.5)).InPiBF(), -1.5);
  EXPECT_DOUBLE_EQ(EiBS(RunTimeNum(-1.5)).InEiBF(), -1.5);
}

TEST(ByteSizeDeltaTest, ConstructionOtherUnitConversionOrder) {
  // The max and min int32_t held in a wider type so multiplication doesn't
  // overflow.
  constexpr int64_t kMaxI32 = std::numeric_limits<int32_t>::max();
  constexpr int64_t kMinI32 = std::numeric_limits<int32_t>::min();

  // 32-bit numbers that will overflow/underflow if multiplied as
  // CheckedNumeric<int32_t> but not if multiplied as CheckedNumeric<int64_t>.
  // This verifies that the implementations of the templates avoid a subtle
  // conversion order bug. Note that dividing kMaxI32 by 1 TiB or higher rounds
  // down to 1, which isn't useful.
  constexpr int32_t kLargeKiB = kMaxI32 / 1024 + 1;
  constexpr int32_t kLargeMiB = kMaxI32 / 1024 / 1024 + 1;
  constexpr int32_t kLargeGiB = kMaxI32 / 1024 / 1024 / 1024 + 1;
  constexpr int32_t kSmallKiB = kMinI32 / 1024 - 1;
  constexpr int32_t kSmallMiB = kMinI32 / 1024 / 1024 - 1;
  constexpr int32_t kSmallGiB = kMinI32 / 1024 / 1024 / 1024 - 1;

  // The expected results of converting the numbers above to bytes.
  constexpr int64_t kExpectedLargeKiB = static_cast<int64_t>(kLargeKiB) * 1024;
  constexpr int64_t kExpectedLargeMiB =
      static_cast<int64_t>(kLargeMiB) * 1024 * 1024;
  constexpr int64_t kExpectedLargeGiB =
      static_cast<int64_t>(kLargeGiB) * 1024 * 1024 * 1024;
  constexpr int64_t kExpectedSmallKiB = static_cast<int64_t>(kSmallKiB) * 1024;
  constexpr int64_t kExpectedSmallMiB =
      static_cast<int64_t>(kSmallMiB) * 1024 * 1024;
  constexpr int64_t kExpectedSmallGiB =
      static_cast<int64_t>(kSmallGiB) * 1024 * 1024 * 1024;

  static_assert(kExpectedLargeKiB > kMaxI32);
  static_assert(kExpectedLargeMiB > kMaxI32);
  static_assert(kExpectedLargeGiB > kMaxI32);
  static_assert(kExpectedSmallKiB < kMinI32);
  static_assert(kExpectedSmallMiB < kMinI32);
  static_assert(kExpectedSmallGiB < kMinI32);

  EXPECT_EQ(KiBS(kLargeKiB).InBytes(), kExpectedLargeKiB);
  EXPECT_EQ(KiBS(1.0 * kLargeKiB).InBytes(), kExpectedLargeKiB);
  EXPECT_DOUBLE_EQ(KiBS(1.0 * kLargeKiB).InBytesF(), 1.0 * kExpectedLargeKiB);
  EXPECT_EQ(KiBS(kSmallKiB).InBytes(), kExpectedSmallKiB);
  EXPECT_EQ(KiBS(1.0 * kSmallKiB).InBytes(), kExpectedSmallKiB);
  EXPECT_DOUBLE_EQ(KiBS(1.0 * kSmallKiB).InBytesF(), 1.0 * kExpectedSmallKiB);

  EXPECT_EQ(MiBS(kLargeMiB).InBytes(), kExpectedLargeMiB);
  EXPECT_EQ(MiBS(1.0 * kLargeMiB).InBytes(), kExpectedLargeMiB);
  EXPECT_DOUBLE_EQ(MiBS(1.0 * kLargeMiB).InBytesF(), 1.0 * kExpectedLargeMiB);
  EXPECT_EQ(MiBS(kSmallMiB).InBytes(), kExpectedSmallMiB);
  EXPECT_EQ(MiBS(1.0 * kSmallMiB).InBytes(), kExpectedSmallMiB);
  EXPECT_DOUBLE_EQ(MiBS(1.0 * kSmallMiB).InBytesF(), 1.0 * kExpectedSmallMiB);

  EXPECT_EQ(GiBS(kLargeGiB).InBytes(), kExpectedLargeGiB);
  EXPECT_EQ(GiBS(1.0 * kLargeGiB).InBytes(), kExpectedLargeGiB);
  EXPECT_DOUBLE_EQ(GiBS(1.0 * kLargeGiB).InBytesF(), 1.0 * kExpectedLargeGiB);
  EXPECT_EQ(GiBS(kSmallGiB).InBytes(), kExpectedSmallGiB);
  EXPECT_EQ(GiBS(1.0 * kSmallGiB).InBytes(), kExpectedSmallGiB);
  EXPECT_DOUBLE_EQ(GiBS(1.0 * kSmallGiB).InBytesF(), 1.0 * kExpectedSmallGiB);
}

TEST(ByteSizeDeltaDeathTest, ConstructionOtherUnitOutOfRange) {
  // TODO(crbug.com/448661443): Detect out-of-range signed constants at compile
  // time.
  BASE_EXPECT_DEATH(KiBS(kMinByteSizeDelta), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(MiBS(kMinByteSizeDelta), "");
    BASE_EXPECT_DEATH(GiBS(kMinByteSizeDelta), "");
    BASE_EXPECT_DEATH(TiBS(kMinByteSizeDelta), "");
    BASE_EXPECT_DEATH(PiBS(kMinByteSizeDelta), "");
    BASE_EXPECT_DEATH(EiBS(kMinByteSizeDelta), "");
  }

  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(KiBS(static_cast<double>(kMinByteSizeDelta)), "");
    BASE_EXPECT_DEATH(MiBS(static_cast<double>(kMinByteSizeDelta)), "");
    BASE_EXPECT_DEATH(GiBS(static_cast<double>(kMinByteSizeDelta)), "");
    BASE_EXPECT_DEATH(TiBS(static_cast<double>(kMinByteSizeDelta)), "");
    BASE_EXPECT_DEATH(PiBS(static_cast<double>(kMinByteSizeDelta)), "");
    BASE_EXPECT_DEATH(EiBS(static_cast<double>(kMinByteSizeDelta)), "");
  }

  BASE_EXPECT_DEATH(KiBS(kMaxByteSizeDelta), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(MiBS(kMaxByteSizeDelta), "");
    BASE_EXPECT_DEATH(GiBS(kMaxByteSizeDelta), "");
    BASE_EXPECT_DEATH(TiBS(kMaxByteSizeDelta), "");
    BASE_EXPECT_DEATH(PiBS(kMaxByteSizeDelta), "");
    BASE_EXPECT_DEATH(EiBS(kMaxByteSizeDelta), "");
  }

  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(KiBS(static_cast<double>(kMaxByteSizeDelta)), "");
    BASE_EXPECT_DEATH(MiBS(static_cast<double>(kMaxByteSizeDelta)), "");
    BASE_EXPECT_DEATH(GiBS(static_cast<double>(kMaxByteSizeDelta)), "");
    BASE_EXPECT_DEATH(TiBS(static_cast<double>(kMaxByteSizeDelta)), "");
    BASE_EXPECT_DEATH(PiBS(static_cast<double>(kMaxByteSizeDelta)), "");
    BASE_EXPECT_DEATH(EiBS(static_cast<double>(kMaxByteSizeDelta)), "");
  }
}

TEST(ByteSizeDeltaTest, IsZero) {
  EXPECT_TRUE(ByteSizeDelta(0).is_zero());
  EXPECT_FALSE(ByteSizeDelta(2).is_zero());
  EXPECT_FALSE(ByteSizeDelta(-2).is_zero());
  EXPECT_FALSE(ByteSizeDelta::Max().is_zero());
  EXPECT_FALSE(ByteSizeDelta::Min().is_zero());
}

TEST(ByteSizeDeltaTest, IsPositive) {
  EXPECT_FALSE(ByteSizeDelta(0).is_positive());
  EXPECT_TRUE(ByteSizeDelta(2).is_positive());
  EXPECT_FALSE(ByteSizeDelta(-2).is_positive());
  EXPECT_TRUE(ByteSizeDelta::Max().is_positive());
  EXPECT_FALSE(ByteSizeDelta::Min().is_positive());
}

TEST(ByteSizeDeltaTest, IsNegative) {
  EXPECT_FALSE(ByteSizeDelta(0).is_negative());
  EXPECT_FALSE(ByteSizeDelta(2).is_negative());
  EXPECT_TRUE(ByteSizeDelta(-2).is_negative());
  EXPECT_FALSE(ByteSizeDelta::Max().is_negative());
  EXPECT_TRUE(ByteSizeDelta::Min().is_negative());
}

TEST(ByteSizeDeltaTest, IsMax) {
  EXPECT_FALSE(ByteSizeDelta(0).is_max());
  EXPECT_FALSE(ByteSizeDelta(2).is_max());
  EXPECT_FALSE(ByteSizeDelta(-2).is_max());
  EXPECT_TRUE(ByteSizeDelta::Max().is_max());
  EXPECT_FALSE(ByteSizeDelta::Min().is_max());
}

TEST(ByteSizeDeltaTest, IsMin) {
  EXPECT_FALSE(ByteSizeDelta(0).is_min());
  EXPECT_FALSE(ByteSizeDelta(2).is_min());
  EXPECT_FALSE(ByteSizeDelta(-2).is_min());
  EXPECT_FALSE(ByteSizeDelta::Max().is_min());
  EXPECT_TRUE(ByteSizeDelta::Min().is_min());
}

TEST(ByteSizeDeltaTest, InFloating) {
  constexpr ByteSizeDelta bytes(3435973836);
  EXPECT_DOUBLE_EQ(bytes.InBytesF(), 3435973836.0);
  EXPECT_DOUBLE_EQ(bytes.InKiBF(), 3355443.19921875);
  EXPECT_DOUBLE_EQ(bytes.InMiBF(), 3276.7999992370605);
  EXPECT_DOUBLE_EQ(bytes.InGiBF(), 3.1999999992549419);
  constexpr ByteSizeDelta more_bytes(3435973836343597383);
  EXPECT_DOUBLE_EQ(more_bytes.InTiBF(), 3124999.9995849044);
  EXPECT_DOUBLE_EQ(more_bytes.InPiBF(), 3051.7578120946332);
  EXPECT_DOUBLE_EQ(more_bytes.InEiBF(), 2.9802322383736652);
  constexpr ByteSizeDelta negative(-7658724168);
  EXPECT_DOUBLE_EQ(negative.InBytesF(), -7658724168.0);
  EXPECT_DOUBLE_EQ(negative.InKiBF(), -7479222.8203125);
  EXPECT_DOUBLE_EQ(negative.InMiBF(), -7303.928535461426);
  EXPECT_DOUBLE_EQ(negative.InGiBF(), -7.132742710411549);
  constexpr ByteSizeDelta more_negative(-7658724168389730908);
  EXPECT_DOUBLE_EQ(more_negative.InTiBF(), -6965569.0534907365);
  EXPECT_DOUBLE_EQ(more_negative.InPiBF(), -6802.313528799547);
  EXPECT_DOUBLE_EQ(more_negative.InEiBF(), -6.642884305468308);
}

TEST(ByteSizeDeltaDeathTest, Abs) {
  // The min two's complement integer has no corresponding positive value.
  BASE_EXPECT_DEATH(ByteSizeDelta::Min().Abs(), "");
  EXPECT_EQ(kByteSizeDeltaNegativeMax.Abs(), ByteSizeDelta::Max());
  EXPECT_EQ(ByteSizeDelta(-79).Abs(), ByteSizeDelta(79));
  EXPECT_EQ(ByteSizeDelta().Abs(), ByteSizeDelta());
  EXPECT_EQ(ByteSizeDelta(79).Abs(), ByteSizeDelta(79));
  EXPECT_EQ(ByteSizeDelta::Max().Abs(), ByteSizeDelta::Max());
}

TEST(ByteSizeDeltaDeathTest, Magnitude) {
  // The min two's complement integer has no corresponding positive value.
  BASE_EXPECT_DEATH(ByteSizeDelta::Min().Magnitude(), "");
  EXPECT_EQ(kByteSizeDeltaNegativeMax.Magnitude(), ByteSize::Max());
  EXPECT_EQ(ByteSizeDelta(-79).Magnitude(), ByteSize(79));
  EXPECT_EQ(ByteSizeDelta().Magnitude(), ByteSize());
  EXPECT_EQ(ByteSizeDelta(79).Magnitude(), ByteSize(79));
  EXPECT_EQ(ByteSizeDelta::Max().Magnitude(), ByteSize::Max());
}

TEST(ByteSizeDeltaTest, UnaryPlus) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-79),
        ByteSizeDelta(), ByteSizeDelta(79), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(+delta, delta);
  }
}

TEST(ByteSizeDeltaDeathTest, UnaryMinus) {
  // The min two's complement integer has no corresponding positive value.
  BASE_EXPECT_DEATH(-ByteSizeDelta::Min(), "");
  EXPECT_EQ(-kByteSizeDeltaNegativeMax, ByteSizeDelta::Max());
  EXPECT_EQ(-ByteSizeDelta(-79), ByteSizeDelta(79));
  EXPECT_EQ(-ByteSizeDelta(), ByteSizeDelta());
  EXPECT_EQ(-ByteSizeDelta(79), ByteSizeDelta(-79));
  EXPECT_EQ(-ByteSizeDelta::Max(), kByteSizeDeltaNegativeMax);
}

// ByteSizeDelta + ByteSizeDelta -> ByteSizeDelta, range [min...max]

TEST(ByteSizeDeltaDeathTest, AddByteSizeDeltaMin) {
  // Add min (equivalent to subtracting max+1).
  if (kRunAllDeathTests) {
    // Make sure min + min doesn't wrap around.
    ByteSizeDelta min_delta = ByteSizeDelta::Min();
    BASE_EXPECT_DEATH(min_delta + ByteSizeDelta::Min(), "");
    BASE_EXPECT_DEATH(min_delta += ByteSizeDelta::Min(), "");

    ByteSizeDelta negative(-1);
    BASE_EXPECT_DEATH(negative + ByteSizeDelta::Min(), "");
    BASE_EXPECT_DEATH(negative += ByteSizeDelta::Min(), "");
  }

  ByteSizeDelta zero;
  EXPECT_EQ(zero + ByteSizeDelta::Min(), ByteSizeDelta::Min());
  zero += ByteSizeDelta::Min();
  EXPECT_EQ(zero, ByteSizeDelta::Min());

  ByteSizeDelta positive(1);
  EXPECT_EQ(positive + ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax);
  positive += ByteSizeDelta::Min();
  EXPECT_EQ(positive, kByteSizeDeltaNegativeMax);

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta + ByteSizeDelta::Min(), ByteSizeDelta(-1));
  max_delta += ByteSizeDelta::Min();
  EXPECT_EQ(max_delta, ByteSizeDelta(-1));
}

TEST(ByteSizeDeltaDeathTest, AddByteSizeDeltaNegativeMax) {
  // Add -max (equivalent to subtracting max).
  if (kRunAllDeathTests) {
    // Make sure min - max doesn't wrap around.
    ByteSizeDelta min_delta = ByteSizeDelta::Min();
    BASE_EXPECT_DEATH(min_delta + kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(min_delta += kByteSizeDeltaNegativeMax, "");
  }

  ByteSizeDelta negative(-1);
  EXPECT_EQ(negative + kByteSizeDeltaNegativeMax, ByteSizeDelta::Min());
  negative += kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative, ByteSizeDelta::Min());

  ByteSizeDelta zero;
  EXPECT_EQ(zero + kByteSizeDeltaNegativeMax, kByteSizeDeltaNegativeMax);
  zero += kByteSizeDeltaNegativeMax;
  EXPECT_EQ(zero, kByteSizeDeltaNegativeMax);

  ByteSizeDelta positive(kMaxByteSizeDelta - 10);
  EXPECT_EQ(positive + kByteSizeDeltaNegativeMax, ByteSizeDelta(-10));
  positive += kByteSizeDeltaNegativeMax;
  EXPECT_EQ(positive, ByteSizeDelta(-10));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta + kByteSizeDeltaNegativeMax, ByteSizeDelta());
  max_delta += kByteSizeDeltaNegativeMax;
  EXPECT_EQ(max_delta, ByteSizeDelta());
}

TEST(ByteSizeDeltaDeathTest, AddByteSizeDeltaNegative) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta + ByteSizeDelta(-1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(min_delta += ByteSizeDelta(-1), "");
  }

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative + ByteSizeDelta(-10), ByteSizeDelta(-52));
  negative += ByteSizeDelta(-2);
  EXPECT_EQ(negative, ByteSizeDelta(-44));

  ByteSizeDelta zero;
  EXPECT_EQ(zero + ByteSizeDelta(-10), ByteSizeDelta(-10));
  zero += ByteSizeDelta(-2);
  EXPECT_EQ(zero, ByteSizeDelta(-2));

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive + ByteSizeDelta(-10), ByteSizeDelta(32));
  EXPECT_EQ(positive + ByteSizeDelta(-42), ByteSizeDelta());
  EXPECT_EQ(positive + ByteSizeDelta(-52), ByteSizeDelta(-10));
  {
    ByteSizeDelta positive2 = positive;
    positive2 += ByteSizeDelta(-10);
    EXPECT_EQ(positive2, ByteSizeDelta(32));
  }
  {
    ByteSizeDelta positive2 = positive;
    positive2 += ByteSizeDelta(-42);
    EXPECT_EQ(positive2, ByteSizeDelta());
  }
  positive += ByteSizeDelta(-52);
  EXPECT_EQ(positive, ByteSizeDelta(-10));
}

TEST(ByteSizeDeltaTest, AddByteSizeDeltaZero) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(), ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta + ByteSizeDelta(), delta);
    ByteSizeDelta delta2 = delta;
    delta2 += ByteSizeDelta();
    EXPECT_EQ(delta2, delta);
  }
}

TEST(ByteSizeDeltaDeathTest, AddByteSizeDeltaPositive) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta + ByteSizeDelta(1), kByteSizeDeltaNegativeMax);
  min_delta += ByteSizeDelta(1);
  EXPECT_EQ(min_delta, kByteSizeDeltaNegativeMax);

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative + ByteSizeDelta(10), ByteSizeDelta(-32));
  EXPECT_EQ(negative + ByteSizeDelta(42), ByteSizeDelta());
  EXPECT_EQ(negative + ByteSizeDelta(52), ByteSizeDelta(10));
  {
    ByteSizeDelta negative2 = negative;
    negative2 += ByteSizeDelta(10);
    EXPECT_EQ(negative2, ByteSizeDelta(-32));
  }
  {
    ByteSizeDelta negative2 = negative;
    negative2 += ByteSizeDelta(42);
    EXPECT_EQ(negative2, ByteSizeDelta());
  }
  negative += ByteSizeDelta(52);
  EXPECT_EQ(negative, ByteSizeDelta(10));

  ByteSizeDelta zero;
  EXPECT_EQ(zero + ByteSizeDelta(12), ByteSizeDelta(12));
  zero += ByteSizeDelta(12);
  EXPECT_EQ(zero, ByteSizeDelta(12));

  ByteSizeDelta positive(kMaxByteSizeDelta - 1);
  EXPECT_EQ(positive + ByteSizeDelta(1), ByteSizeDelta::Max());
  positive += ByteSizeDelta(1);
  EXPECT_EQ(positive, ByteSizeDelta::Max());

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  BASE_EXPECT_DEATH(max_delta + ByteSizeDelta(1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(max_delta += ByteSizeDelta(1), "");
  }
}

TEST(ByteSizeDeltaDeathTest, AddByteSizeDeltaMax) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta + ByteSizeDelta::Max(), ByteSizeDelta(-1));
  min_delta += ByteSizeDelta::Max();
  EXPECT_EQ(min_delta, ByteSizeDelta(-1));

  ByteSizeDelta negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative + ByteSizeDelta::Max(), ByteSizeDelta());
  negative += ByteSizeDelta::Max();
  EXPECT_EQ(negative, ByteSizeDelta());

  ByteSizeDelta zero;
  EXPECT_EQ(zero + ByteSizeDelta::Max(), ByteSizeDelta::Max());
  zero += ByteSizeDelta::Max();
  EXPECT_EQ(zero, ByteSizeDelta::Max());

  ByteSizeDelta positive(1);
  BASE_EXPECT_DEATH(positive + ByteSizeDelta::Max(), "");
  BASE_EXPECT_DEATH(positive += ByteSizeDelta::Max(), "");

  if (kRunAllDeathTests) {
    // Make sure max + max doesn't wrap around.
    ByteSizeDelta max_delta = ByteSizeDelta::Max();
    BASE_EXPECT_DEATH(max_delta + ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(max_delta += ByteSizeDelta::Max(), "");
  }
}

// ByteSizeDelta - ByteSizeDelta -> ByteSizeDelta, range [min...max]

TEST(ByteSizeDeltaDeathTest, SubtractByteSizeDeltaMin) {
  // Subtract min (equivalent to adding max+1).
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta - ByteSizeDelta::Min(), ByteSizeDelta());
  min_delta -= ByteSizeDelta::Min();
  EXPECT_EQ(min_delta, ByteSizeDelta());

  ByteSizeDelta negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative - ByteSizeDelta::Min(), ByteSizeDelta(1));
  negative -= ByteSizeDelta::Min();
  EXPECT_EQ(negative, ByteSizeDelta(1));

  if (kRunAllDeathTests) {
    ByteSizeDelta zero;
    BASE_EXPECT_DEATH(zero - ByteSizeDelta::Min(), "");
    BASE_EXPECT_DEATH(zero -= ByteSizeDelta::Min(), "");

    // Make sure max + max + 1 doesn't wrap around.
    ByteSizeDelta max_delta = ByteSizeDelta::Max();
    BASE_EXPECT_DEATH(max_delta - ByteSizeDelta::Min(), "");
    BASE_EXPECT_DEATH(max_delta -= ByteSizeDelta::Min(), "");
  }
}

TEST(ByteSizeDeltaDeathTest, SubtractByteSizeDeltaNegativeMax) {
  // Subtract -max (equivalent to adding max).
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta - kByteSizeDeltaNegativeMax, ByteSizeDelta(-1));
  min_delta -= kByteSizeDeltaNegativeMax;
  EXPECT_EQ(min_delta, ByteSizeDelta(-1));

  ByteSizeDelta negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative - kByteSizeDeltaNegativeMax, ByteSizeDelta());
  negative -= kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative, ByteSizeDelta());

  ByteSizeDelta zero;
  EXPECT_EQ(zero - kByteSizeDeltaNegativeMax, ByteSizeDelta::Max());
  zero -= kByteSizeDeltaNegativeMax;
  EXPECT_EQ(zero, ByteSizeDelta::Max());

  if (kRunAllDeathTests) {
    ByteSizeDelta positive(1);
    BASE_EXPECT_DEATH(positive - kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(positive -= kByteSizeDeltaNegativeMax, "");

    // Make sure max + max doesn't wrap around.
    ByteSizeDelta max_delta = ByteSizeDelta::Max();
    BASE_EXPECT_DEATH(max_delta - kByteSizeDeltaNegativeMax, "");
    BASE_EXPECT_DEATH(max_delta -= kByteSizeDeltaNegativeMax, "");
  }
}

TEST(ByteSizeDeltaDeathTest, SubtractByteSizeDeltaNegative) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta - ByteSizeDelta(-1), kByteSizeDeltaNegativeMax);
  min_delta -= ByteSizeDelta(-1);
  EXPECT_EQ(min_delta, kByteSizeDeltaNegativeMax);

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative - ByteSizeDelta(-10), ByteSizeDelta(-32));
  EXPECT_EQ(negative - ByteSizeDelta(-42), ByteSizeDelta());
  {
    ByteSizeDelta negative2 = negative;
    negative2 -= ByteSizeDelta(-10);
    EXPECT_EQ(negative2, ByteSizeDelta(-32));
  }
  negative -= ByteSizeDelta(-42);
  EXPECT_EQ(negative, ByteSizeDelta());

  ByteSizeDelta zero;
  EXPECT_EQ(zero - ByteSizeDelta(-10), ByteSizeDelta(10));
  zero -= ByteSizeDelta(-2);
  EXPECT_EQ(zero, ByteSizeDelta(2));

  ByteSizeDelta positive(kMaxByteSizeDelta - 100);
  EXPECT_EQ(positive - ByteSizeDelta(-100), ByteSizeDelta::Max());
  positive -= ByteSizeDelta(-100);
  EXPECT_EQ(positive, ByteSizeDelta::Max());

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  BASE_EXPECT_DEATH(max_delta - ByteSizeDelta(-1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(max_delta -= ByteSizeDelta(-1), "");
  }
}

TEST(ByteSizeDeltaTest, SubtractByteSizeDeltaZero) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(), ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta - ByteSizeDelta(), delta);
    ByteSizeDelta delta2 = delta;
    delta2 -= ByteSizeDelta();
    EXPECT_EQ(delta2, delta);
  }
}

TEST(ByteSizeDeltaDeathTest, SubtractByteSizeDeltaPositive) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta - ByteSizeDelta(1), "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(min_delta -= ByteSizeDelta(1), "");
  }

  ByteSizeDelta negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(negative - ByteSizeDelta(1), ByteSizeDelta::Min());
  negative -= ByteSizeDelta(1);
  EXPECT_EQ(negative, ByteSizeDelta::Min());

  ByteSizeDelta zero;
  EXPECT_EQ(zero - ByteSizeDelta(12), ByteSizeDelta(-12));
  zero -= ByteSizeDelta(12);
  EXPECT_EQ(zero, ByteSizeDelta(-12));

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive - ByteSizeDelta(32), ByteSizeDelta(10));
  EXPECT_EQ(positive - ByteSizeDelta(42), ByteSizeDelta());
  EXPECT_EQ(positive - ByteSizeDelta(52), ByteSizeDelta(-10));
  {
    ByteSizeDelta positive2 = positive;
    positive2 -= ByteSizeDelta(32);
    EXPECT_EQ(positive2, ByteSizeDelta(10));
  }
  {
    ByteSizeDelta positive2 = positive;
    positive2 -= ByteSizeDelta(42);
    EXPECT_EQ(positive2, ByteSizeDelta());
  }
  positive -= ByteSizeDelta(52);
  EXPECT_EQ(positive, ByteSizeDelta(-10));
}

TEST(ByteSizeDeltaDeathTest, SubtractByteSizeDeltaMax) {
  if (kRunAllDeathTests) {
    // Make sure min - max doesn't wrap around.
    ByteSizeDelta min_delta = ByteSizeDelta::Min();
    BASE_EXPECT_DEATH(min_delta - ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(min_delta -= ByteSizeDelta::Max(), "");

    ByteSizeDelta small_negative = ByteSizeDelta(-2);
    BASE_EXPECT_DEATH(small_negative - ByteSizeDelta::Max(), "");
    BASE_EXPECT_DEATH(small_negative -= ByteSizeDelta::Max(), "");
  }

  ByteSizeDelta negative = ByteSizeDelta(-1);
  EXPECT_EQ(negative - ByteSizeDelta::Max(), ByteSizeDelta::Min());
  negative -= ByteSizeDelta::Max();
  EXPECT_EQ(negative, ByteSizeDelta::Min());

  ByteSizeDelta zero;
  EXPECT_EQ(zero - ByteSizeDelta::Max(), kByteSizeDeltaNegativeMax);
  zero -= ByteSizeDelta::Max();
  EXPECT_EQ(zero, kByteSizeDeltaNegativeMax);

  ByteSizeDelta positive(kMaxByteSizeDelta - 1);
  EXPECT_EQ(positive - ByteSizeDelta::Max(), ByteSizeDelta(-1));
  positive -= ByteSizeDelta::Max();
  EXPECT_EQ(positive, ByteSizeDelta(-1));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta - ByteSizeDelta::Max(), ByteSizeDelta());
  max_delta -= ByteSizeDelta::Max();
  EXPECT_EQ(max_delta, ByteSizeDelta());
}

TEST(ByteSizeDeltaTest, MultiplyByZero) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(), ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta * 0, ByteSizeDelta());
    EXPECT_EQ(0 * delta, ByteSizeDelta());
    delta *= 0;
    EXPECT_EQ(delta, ByteSizeDelta());
  }
}

TEST(ByteSizeDeltaTest, MultiplyIdentity) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(), ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta * 1, delta);
    EXPECT_EQ(1 * delta, delta);
    ByteSizeDelta delta2 = delta;
    delta2 *= 1;
    EXPECT_EQ(delta2, delta);
  }
}

TEST(ByteSizeDeltaDeathTest, MultiplyInvert) {
  // The min two's complement integer has no corresponding positive value.
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta * -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(-1 * min_delta, "");
    BASE_EXPECT_DEATH(min_delta *= -1, "");
  }

  ByteSizeDelta max_negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(max_negative * -1, ByteSizeDelta::Max());
  EXPECT_EQ(-1 * max_negative, ByteSizeDelta::Max());
  max_negative *= -1;
  EXPECT_EQ(max_negative, ByteSizeDelta::Max());

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative * -1, ByteSizeDelta(42));
  EXPECT_EQ(-1 * negative, ByteSizeDelta(42));
  negative *= -1;
  EXPECT_EQ(negative, ByteSizeDelta(42));

  ByteSizeDelta zero;
  EXPECT_EQ(zero * -1, ByteSizeDelta());
  EXPECT_EQ(-1 * zero, ByteSizeDelta());
  zero *= -1;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive * -1, ByteSizeDelta(-42));
  EXPECT_EQ(-1 * positive, ByteSizeDelta(-42));
  positive *= -1;
  EXPECT_EQ(positive, ByteSizeDelta(-42));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta * -1, kByteSizeDeltaNegativeMax);
  EXPECT_EQ(-1 * max_delta, kByteSizeDeltaNegativeMax);
  max_delta *= -1;
  EXPECT_EQ(max_delta, kByteSizeDeltaNegativeMax);
}

TEST(ByteSizeDeltaDeathTest, MultiplyPositive) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta * 2, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(2 * min_delta, "");
    BASE_EXPECT_DEATH(min_delta *= 2, "");
  }

  if (kRunAllDeathTests) {
    ByteSizeDelta max_negative = kByteSizeDeltaNegativeMax;
    BASE_EXPECT_DEATH(max_negative * 2, "");
    BASE_EXPECT_DEATH(2 * max_negative, "");
    BASE_EXPECT_DEATH(max_negative *= 2, "");
  }

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative * 2, ByteSizeDelta(-84));
  EXPECT_EQ(2 * negative, ByteSizeDelta(-84));
  negative *= 2;
  EXPECT_EQ(negative, ByteSizeDelta(-84));

  ByteSizeDelta zero;
  EXPECT_EQ(zero * 2, ByteSizeDelta());
  EXPECT_EQ(2 * zero, ByteSizeDelta());
  zero *= 2;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive * 2, ByteSizeDelta(84));
  EXPECT_EQ(2 * positive, ByteSizeDelta(84));
  positive *= 2;
  EXPECT_EQ(positive, ByteSizeDelta(84));

  ByteSizeDelta large_positive = ByteSizeDelta(kMaxByteSizeDelta - 100);
  BASE_EXPECT_DEATH(large_positive * 2, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(2 * large_positive, "");
    BASE_EXPECT_DEATH(large_positive *= 2, "");
  }

  if (kRunAllDeathTests) {
    ByteSizeDelta max_delta = ByteSizeDelta::Max();
    BASE_EXPECT_DEATH(max_delta * 2, "");
    BASE_EXPECT_DEATH(2 * max_delta, "");
    BASE_EXPECT_DEATH(max_delta *= 2, "");
  }
}

TEST(ByteSizeDeltaDeathTest, MultiplyNegative) {
  // Overflows in the positive direction.
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta * -2, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(-2 * min_delta, "");
    BASE_EXPECT_DEATH(min_delta *= -2, "");
  }

  if (kRunAllDeathTests) {
    ByteSizeDelta max_negative = kByteSizeDeltaNegativeMax;
    BASE_EXPECT_DEATH(max_negative * -2, "");
    BASE_EXPECT_DEATH(-2 * max_negative, "");
    BASE_EXPECT_DEATH(max_negative *= -2, "");
  }

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative * -2, ByteSizeDelta(84));
  EXPECT_EQ(-2 * negative, ByteSizeDelta(84));
  negative *= -2;
  EXPECT_EQ(negative, ByteSizeDelta(84));

  ByteSizeDelta zero;
  EXPECT_EQ(zero * -2, ByteSizeDelta());
  EXPECT_EQ(-2 * zero, ByteSizeDelta());
  zero *= -2;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive * -2, ByteSizeDelta(-84));
  EXPECT_EQ(-2 * positive, ByteSizeDelta(-84));
  positive *= -2;
  EXPECT_EQ(positive, ByteSizeDelta(-84));

  // Overflows in the negative direction.
  ByteSizeDelta large_positive = ByteSizeDelta(kMaxByteSizeDelta - 100);
  BASE_EXPECT_DEATH(large_positive * -2, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(-2 * large_positive, "");
    BASE_EXPECT_DEATH(large_positive *= -2, "");
  }

  if (kRunAllDeathTests) {
    ByteSizeDelta max_delta = ByteSizeDelta::Max();
    BASE_EXPECT_DEATH(max_delta * -2, "");
    BASE_EXPECT_DEATH(-2 * max_delta, "");
    BASE_EXPECT_DEATH(max_delta *= -2, "");
  }
}

TEST(ByteSizeDeltaTest, DivideIdentity) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(), ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta / 1, delta);
    ByteSizeDelta delta2 = delta;
    delta2 /= 1;
    EXPECT_EQ(delta2, delta);
  }
}

TEST(ByteSizeDeltaTest, DivideBySelf) {
  for (auto delta :
       {ByteSizeDelta::Min(), kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
        ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    EXPECT_EQ(delta / delta.InBytes(), ByteSizeDelta(1));
    delta /= delta.InBytes();
    EXPECT_EQ(delta, ByteSizeDelta(1));
  }
}

TEST(ByteSizeDeltaTest, DivideByInverse) {
  // The min two's complement integer has no corresponding positive value, so
  // can't be tested.
  for (auto delta : {kByteSizeDeltaNegativeMax, ByteSizeDelta(-42),
                     ByteSizeDelta(42), ByteSizeDelta::Max()}) {
    SCOPED_TRACE(delta);
    int64_t inverse = delta.InBytes() * -1;
    EXPECT_EQ(delta / inverse, ByteSizeDelta(-1));
    delta /= inverse;
    EXPECT_EQ(delta, ByteSizeDelta(-1));
  }
}

TEST(ByteSizeDeltaDeathTest, DivideInvert) {
  // The min two's complement integer has no corresponding positive value.
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  BASE_EXPECT_DEATH(min_delta / -1, "");
  if (kRunAllDeathTests) {
    BASE_EXPECT_DEATH(min_delta /= -1, "");
  }

  ByteSizeDelta max_negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(max_negative / -1, ByteSizeDelta::Max());
  max_negative /= -1;
  EXPECT_EQ(max_negative, ByteSizeDelta::Max());

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative / -1, ByteSizeDelta(42));
  negative /= -1;
  EXPECT_EQ(negative, ByteSizeDelta(42));

  ByteSizeDelta zero;
  EXPECT_EQ(zero / -1, ByteSizeDelta());
  zero /= -1;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive / -1, ByteSizeDelta(-42));
  positive /= -1;
  EXPECT_EQ(positive, ByteSizeDelta(-42));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta / -1, kByteSizeDeltaNegativeMax);
  max_delta /= -1;
  EXPECT_EQ(max_delta, kByteSizeDeltaNegativeMax);
}

TEST(ByteSizeDeltaTest, DividePositive) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta / 2, ByteSizeDelta(kMinByteSizeDelta / 2));
  min_delta /= 2;
  EXPECT_EQ(min_delta, ByteSizeDelta(kMinByteSizeDelta / 2));

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative / 2, ByteSizeDelta(-21));
  negative /= 2;
  EXPECT_EQ(negative, ByteSizeDelta(-21));

  ByteSizeDelta zero;
  EXPECT_EQ(zero / 2, ByteSizeDelta());
  zero /= 2;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive / 2, ByteSizeDelta(21));
  positive /= 2;
  EXPECT_EQ(positive, ByteSizeDelta(21));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta / 2, ByteSizeDelta(kMaxByteSizeDelta / 2));
  max_delta /= 2;
  EXPECT_EQ(max_delta, ByteSizeDelta(kMaxByteSizeDelta / 2));
}

TEST(ByteSizeDeltaTest, DivideNegative) {
  ByteSizeDelta min_delta = ByteSizeDelta::Min();
  EXPECT_EQ(min_delta / -2, ByteSizeDelta(kMinByteSizeDelta / -2));
  min_delta /= -2;
  EXPECT_EQ(min_delta, ByteSizeDelta(kMinByteSizeDelta / -2));

  ByteSizeDelta max_negative = kByteSizeDeltaNegativeMax;
  EXPECT_EQ(max_negative / -2, ByteSizeDelta(kMaxByteSizeDelta / 2));
  max_negative /= -2;
  EXPECT_EQ(max_negative, ByteSizeDelta(kMaxByteSizeDelta / 2));

  ByteSizeDelta negative(-42);
  EXPECT_EQ(negative / -2, ByteSizeDelta(21));
  negative /= -2;
  EXPECT_EQ(negative, ByteSizeDelta(21));

  ByteSizeDelta zero;
  EXPECT_EQ(zero / -2, ByteSizeDelta());
  zero /= -2;
  EXPECT_EQ(zero, ByteSizeDelta());

  ByteSizeDelta positive(42);
  EXPECT_EQ(positive / -2, ByteSizeDelta(-21));
  positive /= -2;
  EXPECT_EQ(positive, ByteSizeDelta(-21));

  ByteSizeDelta max_delta = ByteSizeDelta::Max();
  EXPECT_EQ(max_delta / -2, ByteSizeDelta(kMaxNegativeByteSizeDelta / 2));
  max_delta /= -2;
  EXPECT_EQ(max_delta, ByteSizeDelta(kMaxNegativeByteSizeDelta / 2));
}

TEST(ByteSizeDeltaTest, Comparison) {
  constexpr ByteSizeDelta a(-1);
  constexpr ByteSizeDelta b(1);
  constexpr ByteSizeDelta c(1);

  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
  EXPECT_FALSE(b < c);

  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(b <= a);
  EXPECT_TRUE(b <= c);

  EXPECT_FALSE(a > b);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b > c);

  EXPECT_FALSE(a >= b);
  EXPECT_TRUE(b >= a);
  EXPECT_TRUE(b >= c);

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(b == c);

  EXPECT_TRUE(a != b);
  EXPECT_FALSE(b != c);

  // Boundary conditions.
  for (ByteSizeDelta other : {a, ByteSizeDelta(), b, ByteSizeDelta::Max()}) {
    SCOPED_TRACE(other);

    EXPECT_TRUE(ByteSizeDelta::Min() < other);
    EXPECT_TRUE(ByteSizeDelta::Min() <= other);
    EXPECT_FALSE(ByteSizeDelta::Min() > other);
    EXPECT_FALSE(ByteSizeDelta::Min() >= other);
    EXPECT_FALSE(ByteSizeDelta::Min() == other);
    EXPECT_TRUE(ByteSizeDelta::Min() != other);

    EXPECT_FALSE(other < ByteSizeDelta::Min());
    EXPECT_FALSE(other <= ByteSizeDelta::Min());
    EXPECT_TRUE(other > ByteSizeDelta::Min());
    EXPECT_TRUE(other >= ByteSizeDelta::Min());
    EXPECT_FALSE(other == ByteSizeDelta::Min());
    EXPECT_TRUE(other != ByteSizeDelta::Min());
  }
  for (ByteSizeDelta other : {ByteSizeDelta::Min(), a}) {
    SCOPED_TRACE(other);

    // `other` is negative.
    EXPECT_FALSE(ByteSizeDelta() < other);
    EXPECT_FALSE(ByteSizeDelta() <= other);
    EXPECT_TRUE(ByteSizeDelta() > other);
    EXPECT_TRUE(ByteSizeDelta() >= other);
    EXPECT_FALSE(ByteSizeDelta() == other);
    EXPECT_TRUE(ByteSizeDelta() != other);

    EXPECT_TRUE(other < ByteSizeDelta());
    EXPECT_TRUE(other <= ByteSizeDelta());
    EXPECT_FALSE(other > ByteSizeDelta());
    EXPECT_FALSE(other >= ByteSizeDelta());
    EXPECT_FALSE(other == ByteSizeDelta());
    EXPECT_TRUE(other != ByteSizeDelta());
  }
  for (ByteSizeDelta other : {b, ByteSizeDelta::Max()}) {
    SCOPED_TRACE(other);

    // `other` is positive.
    EXPECT_TRUE(ByteSizeDelta() < other);
    EXPECT_TRUE(ByteSizeDelta() <= other);
    EXPECT_FALSE(ByteSizeDelta() > other);
    EXPECT_FALSE(ByteSizeDelta() >= other);
    EXPECT_FALSE(ByteSizeDelta() == other);
    EXPECT_TRUE(ByteSizeDelta() != other);

    EXPECT_FALSE(other < ByteSizeDelta());
    EXPECT_FALSE(other <= ByteSizeDelta());
    EXPECT_TRUE(other > ByteSizeDelta());
    EXPECT_TRUE(other >= ByteSizeDelta());
    EXPECT_FALSE(other == ByteSizeDelta());
    EXPECT_TRUE(other != ByteSizeDelta());
  }
  for (ByteSizeDelta other : {ByteSizeDelta::Min(), a, ByteSizeDelta(), b}) {
    SCOPED_TRACE(other);

    EXPECT_FALSE(ByteSizeDelta::Max() < other);
    EXPECT_FALSE(ByteSizeDelta::Max() <= other);
    EXPECT_TRUE(ByteSizeDelta::Max() > other);
    EXPECT_TRUE(ByteSizeDelta::Max() >= other);
    EXPECT_FALSE(ByteSizeDelta::Max() == other);
    EXPECT_TRUE(ByteSizeDelta::Max() != other);

    EXPECT_TRUE(other < ByteSizeDelta::Max());
    EXPECT_TRUE(other <= ByteSizeDelta::Max());
    EXPECT_FALSE(other > ByteSizeDelta::Max());
    EXPECT_FALSE(other >= ByteSizeDelta::Max());
    EXPECT_FALSE(other == ByteSizeDelta::Max());
    EXPECT_TRUE(other != ByteSizeDelta::Max());
  }
}

TEST(ByteSizeDeltaTest, StreamOperator) {
  struct TestValue {
    int64_t bytes;
    const char* expected;
  } kTestValues[] = {
      {-1, "-1B"},
      {0, "0B"},
      {1, "1B"},

      {1024 - 1, "1023B"},
      {1024, "1KiB"},
      {1024 + 1, "1025B (1.001KiB)"},
      {-(1024 - 1), "-1023B"},
      {-(1024), "-1KiB"},
      {-(1024 + 1), "-1025B (-1.001KiB)"},

      {1024 * 1024 - 1, "1048575B (1023.999KiB)"},
      {1024 * 1024, "1MiB"},
      {1024 * 1024 + 1'000, "1049576B (1.001MiB)"},
      {-(1024 * 1024 - 1), "-1048575B (-1023.999KiB)"},
      {-(1024 * 1024), "-1MiB"},
      {-(1024 * 1024 + 1'000), "-1049576B (-1.001MiB)"},

      {1024LL * 1024 * 1024 - 1'000, "1073740824B (1023.999MiB)"},
      {1024LL * 1024 * 1024, "1GiB"},
      {1024LL * 1024 * 1024 + 1'000'000, "1074741824B (1.001GiB)"},

      {1024LL * 1024 * 1024 * 1024 - 1'000'000, "1099510627776B (1023.999GiB)"},
      {1024LL * 1024 * 1024 * 1024, "1TiB"},
      {1024LL * 1024 * 1024 * 1024 + 1'000'000'000,
       "1100511627776B (1.001TiB)"},

      {1024LL * 1024 * 1024 * 1024 * 1024 - 1'000'000'000,
       "1125898906842624B (1023.999TiB)"},
      {1024LL * 1024 * 1024 * 1024 * 1024, "1PiB"},
      {1024LL * 1024 * 1024 * 1024 * 1024 + 1'000'000'000'000,
       "1126899906842624B (1.001PiB)"},

      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024 - 1'000'000'000'000,
       "1152920504606846976B (1023.999PiB)"},
      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024, "1EiB"},
      {1024LL * 1024 * 1024 * 1024 * 1024 * 1024 + 1'000'000'000'000'000,
       "1153921504606846976B (1.001EiB)"},
      {-(1024LL * 1024 * 1024 * 1024 * 1024 * 1024 - 1'000'000'000'000),
       "-1152920504606846976B (-1023.999PiB)"},
      {-(1024LL * 1024 * 1024 * 1024 * 1024 * 1024), "-1EiB"},
      {-(1024LL * 1024 * 1024 * 1024 * 1024 * 1024 + 1'000'000'000'000'000),
       "-1153921504606846976B (-1.001EiB)"},

      {ByteSizeDelta::Max().InBytes(), "9223372036854775807B (8.000EiB)"},
      {ByteSizeDelta::Min().InBytes(), "-8EiB"},
      {ByteSizeDelta::Min().InBytes() + 1, "-9223372036854775807B (-8.000EiB)"},
  };
  for (const auto& test_value : kTestValues) {
    std::stringstream ss;
    ss << ByteSizeDelta(test_value.bytes);
    EXPECT_EQ(test_value.expected, ss.str());
  }
}

}  // namespace base
