// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_interception_apple.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include <mach/mach.h>

#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/malloc_zone_functions_apple.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace allocator_shim {

namespace {
void ResetMallocZone(ChromeMallocZone* zone) {
  MallocZoneFunctions& functions = GetFunctionsForZone(zone);
  ReplaceZoneFunctions(zone, &functions);
}

void ResetAllMallocZones() {
  ChromeMallocZone* default_malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  ResetMallocZone(default_malloc_zone);

  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr = malloc_get_all_zones(mach_task_self(), /*reader=*/nullptr,
                                          &zones, &count);
  if (kr != KERN_SUCCESS) {
    return;
  }
  for (unsigned int i = 0; i < count; ++i) {
    ChromeMallocZone* zone = reinterpret_cast<ChromeMallocZone*>(zones[i]);
    ResetMallocZone(zone);
  }
}
}  // namespace

class AllocatorInterceptionTest : public testing::Test {
 protected:
  void TearDown() override {
    ResetAllMallocZones();
    ClearAllMallocZonesForTesting();
  }
};

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(AllocatorInterceptionTest, ShimNewMallocZones) {
  InitializeAllocatorShim();
  ChromeMallocZone* default_malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());

  malloc_zone_t new_zone;
  // Uninitialized: this is not a problem, because `ShimNewMalloc()` will just
  // copy a member from this struct, not use it, which is why its pointer in
  // `new_zone` must be valid, but its content can be meaningless.
  malloc_introspection_t introspection;
  memset(&new_zone, 1, sizeof(malloc_zone_t));
  new_zone.introspect = &introspection;
  malloc_zone_register(&new_zone);
  EXPECT_NE(new_zone.malloc, default_malloc_zone->malloc);
  ShimNewMallocZones();
  EXPECT_EQ(new_zone.malloc, default_malloc_zone->malloc);

  malloc_zone_unregister(&new_zone);
}
#endif

}  // namespace allocator_shim

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
