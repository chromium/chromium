// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/malloc_zone_functions_apple.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace allocator_shim {

class MallocZoneFunctionsTest : public testing::Test {
 protected:
  void TearDown() override { ClearAllMallocZonesForTesting(); }
};

TEST_F(MallocZoneFunctionsTest, TestDefaultZoneMallocFree) {
  ChromeMallocZone* malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  StoreMallocZone(malloc_zone);
  int* test = reinterpret_cast<int*>(
      g_malloc_zones[0].malloc(malloc_default_zone(), 33));
  test[0] = 1;
  test[1] = 2;
  g_malloc_zones[0].free(malloc_default_zone(), test);
}

TEST_F(MallocZoneFunctionsTest, IsZoneAlreadyStored) {
  ChromeMallocZone* malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  EXPECT_FALSE(IsMallocZoneAlreadyStored(malloc_zone));
  StoreMallocZone(malloc_zone);
  EXPECT_TRUE(IsMallocZoneAlreadyStored(malloc_zone));
}

TEST_F(MallocZoneFunctionsTest, CannotDoubleStoreZone) {
  ChromeMallocZone* malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  StoreMallocZone(malloc_zone);
  StoreMallocZone(malloc_zone);
  EXPECT_EQ(1, GetMallocZoneCountForTesting());
}

TEST_F(MallocZoneFunctionsTest, CannotStoreMoreThanMaxZones) {
  std::vector<ChromeMallocZone> zones;
  zones.resize(kMaxZoneCount * 2);
  for (int i = 0; i < kMaxZoneCount * 2; ++i) {
    ChromeMallocZone& zone = zones[i];
    memcpy(&zone, malloc_default_zone(), sizeof(ChromeMallocZone));
    StoreMallocZone(&zone);
  }

  int max_zone_count = kMaxZoneCount;
  EXPECT_EQ(max_zone_count, GetMallocZoneCountForTesting());
}

}  // namespace allocator_shim
