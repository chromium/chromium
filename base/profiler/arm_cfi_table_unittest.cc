// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/arm_cfi_table.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

bool operator==(const ArmCFITable::FrameEntry& a,
                const ArmCFITable::FrameEntry& b) {
  return a.cfa_offset == b.cfa_offset && a.ra_offset == b.ra_offset;
}

TEST(ArmCFITableTest, Parse) {
  auto parse_cfi = [](std::vector<uint16_t> data) {
    return ArmCFITable::Parse(
        {reinterpret_cast<const uint8_t*>(data.data()), data.size() * 2});
  };

  auto reader = parse_cfi({0x01, 0x00, 0x0, 0x0, 0xffff});
  EXPECT_TRUE(reader);
  EXPECT_EQ(1U, reader->GetTableSizeForTesting());
}

TEST(ArmCFITableTest, FindEntryForAddress) {
  // Input is generated from the CFI file:
  // STACK CFI INIT 1000 500
  // STACK CFI 1002 .cfa: sp 272 + .ra: .cfa -4 + ^ r4: .cfa -16 +
  // STACK CFI 1008 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  // STACK CFI 1040 .cfa: sp 816 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  // STACK CFI 1050 .cfa: sp 816 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  // STACK CFI 1080 .cfa: sp 544 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  //
  // STACK CFI INIT 2000 22
  // STACK CFI 2004 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  // STACK CFI 2008 .cfa: sp 16 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  //
  // STACK CFI INIT 2024 100
  // STACK CFI 2030 .cfa: sp 48 + .ra: .cfa -12 + ^ r4: .cfa -16 + ^
  // STACK CFI 2100 .cfa: sp 64 + .r1: .cfa -0 + ^ r4: .cfa -16 + ^
  //
  // STACK CFI INIT 2200 10
  // STACK CFI 2204 .cfa: sp 44 + .ra: .cfa -8 + ^ r4: .cfa -16 + ^
  const uint16_t input_data[] = {// UNW_INDEX size
                                 0x07, 0x0,

                                 // UNW_INDEX function_addresses (4 byte rows).
                                 0x1000, 0x0, 0x1502, 0x0, 0x2000, 0x0, 0x2024,
                                 0x0, 0x2126, 0x0, 0x2200, 0x0, 0x2212, 0x0,

                                 // UNW_INDEX entry_data_indices (2 byte rows).
                                 0x0, 0xffff, 0xb, 0x10, 0xffff, 0x15, 0xffff,

                                 // UNW_DATA table.
                                 0x5, 0x2, 0x111, 0x8, 0x220, 0x40, 0x330, 0x50,
                                 0x332, 0x80, 0x220, 0x2, 0x4, 0x13, 0x8, 0x13,
                                 0x2, 0xc, 0x33, 0xdc, 0x40, 0x1, 0x4, 0x2e};

  auto reader = ArmCFITable::Parse(
      {reinterpret_cast<const uint8_t*>(input_data), sizeof(input_data) * 2});
  EXPECT_EQ(7U, reader->GetTableSizeForTesting());

  EXPECT_FALSE(reader->FindEntryForAddress(0x01));
  EXPECT_FALSE(reader->FindEntryForAddress(0x100));
  EXPECT_FALSE(reader->FindEntryForAddress(0x1502));
  EXPECT_FALSE(reader->FindEntryForAddress(0x3000));
  EXPECT_FALSE(reader->FindEntryForAddress(0x2212));

  auto expect_frame = [&](ArmCFITable::FrameEntry expected, uintptr_t address) {
    auto result = reader->FindEntryForAddress(address);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(expected, *result);
  };

  expect_frame({0x110, 0x4}, 0x1002);
  expect_frame({0x110, 0x4}, 0x1003);
  expect_frame({0x220, 0x4}, 0x1008);
  expect_frame({0x220, 0x4}, 0x1009);
  expect_frame({0x220, 0x4}, 0x1039);
  expect_frame({0x220, 0x8}, 0x1080);
  expect_frame({0x220, 0x8}, 0x1100);
  expect_frame({0x0, 0x0}, 0x2024);
  expect_frame({0x30, 0xc}, 0x2050);
  expect_frame({0x2c, 0x8}, 0x2208);
  expect_frame({0x2c, 0x8}, 0x2210);
}

TEST(ArmCFITableTest, InvalidTable) {
  auto parse_cfi_and_find =
      [](std::vector<uint16_t> data,
         uintptr_t address) -> absl::optional<ArmCFITable::FrameEntry> {
    auto reader = ArmCFITable::Parse(
        {reinterpret_cast<const uint8_t*>(data.data()), data.size() * 2});
    if (!reader)
      return absl::nullopt;
    return reader->FindEntryForAddress(address);
  };

  // No data.
  EXPECT_FALSE(parse_cfi_and_find({}, 0x0));

  // Empty UNW_INDEX.
  EXPECT_FALSE(parse_cfi_and_find({0x00, 0x00}, 0x0));

  // Missing UNW_INDEX data.
  EXPECT_FALSE(parse_cfi_and_find({0x01, 0x00}, 0x0));

  // No unwind info for address.
  EXPECT_FALSE(parse_cfi_and_find({0x02, 0x00, 0x0, 0x0, 0xffff}, 0x0));

  // entry_data_indices out of bound.
  EXPECT_FALSE(parse_cfi_and_find(
      {
          // UNW_INDEX size
          0x01,
          0x0,
          // UNW_INDEX
          0x1000,
          0x0,
          0x0,
          // UNW_DATA
          0x5,
      },
      0x1000));

  EXPECT_FALSE(parse_cfi_and_find(
      {
          // UNW_INDEX size
          0x01,
          0x0,
          // UNW_INDEX
          0x1000,
          0x0,
          0x0,
      },
      0x1000));

  // Missing CFIDataRow.
  EXPECT_FALSE(parse_cfi_and_find(
      {
          // UNW_INDEX size
          0x01,
          0x0,
          // UNW_INDEX
          0x1000,
          0x0,
          0x0,
          // UNW_DATA
          0x5,
          0x0,
      },
      0x1000));

  // Invalid CFIDataRow.
  EXPECT_FALSE(parse_cfi_and_find(
      {
          // UNW_INDEX size
          0x01,
          0x0,
          // UNW_INDEX
          0x1000,
          0x0,
          0x0,
          // UNW_DATA
          0x1,
          0x2,
          0x0,
      },
      0x1002));
}

}  // namespace base