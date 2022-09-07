// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/reached_addresses_bitset.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

using testing::ElementsAre;
using testing::ElementsAreArray;

constexpr uintptr_t kStartAddress = 0x1000;
constexpr uintptr_t kEndAddress = 0x2000;
constexpr size_t kStorageSize = 512;

class ReachedAddressesBitsetTest : public testing::Test {
 public:
  ReachedAddressesBitsetTest()
      : bitset_(kStartAddress, kEndAddress, storage_, kStorageSize) {
    memset(storage_, 0, kStorageSize * sizeof(uint32_t));
    EXPECT_TRUE(bitset()->GetReachedOffsets().empty());
  }

  ReachedAddressesBitset* bitset() { return &bitset_; }

 private:
  std::atomic<uint32_t> storage_[kStorageSize];
  ReachedAddressesBitset bitset_;
};

TEST_F(ReachedAddressesBitsetTest, RecordStartAddress) {
  bitset()->RecordAddress(kStartAddress);
  EXPECT_THAT(bitset()->GetReachedOffsets(), ElementsAre(0));
}

TEST_F(ReachedAddressesBitsetTest, RecordLastAddress) {
  bitset()->RecordAddress(kEndAddress - 4);
  EXPECT_THAT(bitset()->GetReachedOffsets(),
              ElementsAre(kEndAddress - 4 - kStartAddress));
}

TEST_F(ReachedAddressesBitsetTest, RecordAddressOutsideOfRange_Small) {
  bitset()->RecordAddress(kStartAddress - 4);
  EXPECT_THAT(bitset()->GetReachedOffsets(), ElementsAre());
}

TEST_F(ReachedAddressesBitsetTest, RecordAddressOutsideOfRange_Large) {
  bitset()->RecordAddress(kEndAddress);
  EXPECT_THAT(bitset()->GetReachedOffsets(), ElementsAre());
}

TEST_F(ReachedAddressesBitsetTest, RecordUnalignedAddresses) {
  constexpr uint32_t aligned_offset = 0x100;
  bitset()->RecordAddress(kStartAddress + aligned_offset + 1);
  bitset()->RecordAddress(kStartAddress + aligned_offset + 2);
  bitset()->RecordAddress(kStartAddress + aligned_offset + 3);
  EXPECT_THAT(bitset()->GetReachedOffsets(), ElementsAre(aligned_offset));
}

TEST_F(ReachedAddressesBitsetTest, FillBitsetOneByOne) {
  std::vector<uint32_t> expected_offsets;
  for (uintptr_t address = kStartAddress; address < kEndAddress; address += 4) {
    bitset()->RecordAddress(address);
    expected_offsets.push_back(address - kStartAddress);
    ASSERT_THAT(bitset()->GetReachedOffsets(),
                ElementsAreArray(expected_offsets))
        << "Last added: " << address;
  }
}

}  // namespace android
}  // namespace base
