// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_MAC_DEFAULT_ZONE_H_
#error This header is meant to be included only once by allocator_shim.cc
#endif
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_OVERRIDE_MAC_DEFAULT_ZONE_H_

#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#error This header must be included iff PartitionAlloc-Everywhere is enabled.
#endif

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bits.h"

namespace base {

// Defined in base/allocator/partition_allocator/partition_root.cc
void PartitionAllocMallocHookOnBeforeForkInParent();
void PartitionAllocMallocHookOnAfterForkInParent();
void PartitionAllocMallocHookOnAfterForkInChild();

namespace allocator {

namespace {

// malloc_introspection_t's callback functions for our own zone

kern_return_t MallocIntrospectionEnumerator(task_t task,
                                            void*,
                                            unsigned type_mask,
                                            vm_address_t zone_address,
                                            memory_reader_t reader,
                                            vm_range_recorder_t recorder) {
  // Should enumerate all memory regions allocated by this allocator, but not
  // implemented just because of no use case for now.
  return KERN_FAILURE;
}

size_t MallocIntrospectionGoodSize(malloc_zone_t* zone, size_t size) {
  return base::bits::AlignUp(size, base::kAlignment);
}

boolean_t MallocIntrospectionCheck(malloc_zone_t* zone) {
  // Should check the consistency of the allocator implementing this malloc
  // zone, but not implemented just because of no use case for now.
  return true;
}

void MallocIntrospectionPrint(malloc_zone_t* zone, boolean_t verbose) {
  // Should print the current states of the zone for debugging / investigation
  // purpose, but not implemented just because of no use case for now.
}

void MallocIntrospectionLog(malloc_zone_t* zone, void* address) {
  // Should enable logging of the activities on the given `address`, but not
  // implemented just because of no use case for now.
}

void MallocIntrospectionForceLock(malloc_zone_t* zone) {
  // Called before fork(2) to acquire the lock.
  PartitionAllocMallocHookOnBeforeForkInParent();
}

void MallocIntrospectionForceUnlock(malloc_zone_t* zone) {
  // Called in the parent process after fork(2) to release the lock.
  PartitionAllocMallocHookOnAfterForkInParent();
}

void MallocIntrospectionStatistics(malloc_zone_t* zone,
                                   malloc_statistics_t* stats) {
  // Should report the memory usage correctly, but not implemented just because
  // of no use case for now.
  stats->blocks_in_use = 0;
  stats->size_in_use = 0;
  stats->max_size_in_use = 0;  // High water mark of touched memory
  stats->size_allocated = 0;   // Reserved in memory
}

boolean_t MallocIntrospectionZoneLocked(malloc_zone_t* zone) {
  // Should return true if the underlying PartitionRoot is locked, but not
  // implemented just because this function seems not used effectively.
  return false;
}

boolean_t MallocIntrospectionEnableDischargeChecking(malloc_zone_t* zone) {
  // 'discharge' is not supported.
  return false;
}

void MallocIntrospectionDisableDischargeChecking(malloc_zone_t* zone) {
  // 'discharge' is not supported.
}

void MallocIntrospectionDischarge(malloc_zone_t* zone, void* memory) {
  // 'discharge' is not supported.
}

void MallocIntrospectionEnumerateDischargedPointers(
    malloc_zone_t* zone,
    void (^report_discharged)(void* memory, void* info)) {
  // 'discharge' is not supported.
}

void MallocIntrospectionReinitLock(malloc_zone_t* zone) {
  // Called in a child process after fork(2) to re-initialize the lock.
  PartitionAllocMallocHookOnAfterForkInChild();
}

void MallocIntrospectionPrintTask(task_t task,
                                  unsigned level,
                                  vm_address_t zone_address,
                                  memory_reader_t reader,
                                  print_task_printer_t printer) {
  // Should print the current states of another process's zone for debugging /
  // investigation purpose, but not implemented just because of no use case
  // for now.
}

void MallocIntrospectionTaskStatistics(task_t task,
                                       vm_address_t zone_address,
                                       memory_reader_t reader,
                                       malloc_statistics_t* stats) {
  // Should report the memory usage in another process's zone, but not
  // implemented just because of no use case for now.
  stats->blocks_in_use = 0;
  stats->size_in_use = 0;
  stats->max_size_in_use = 0;  // High water mark of touched memory
  stats->size_allocated = 0;   // Reserved in memory
}

// malloc_zone_t's callback functions for our own zone

size_t MallocZoneSize(malloc_zone_t* zone, const void* ptr) {
  return ShimGetSizeEstimate(ptr, nullptr);
}

void* MallocZoneMalloc(malloc_zone_t* zone, size_t size) {
  return ShimMalloc(size, nullptr);
}

void* MallocZoneCalloc(malloc_zone_t* zone, size_t n, size_t size) {
  return ShimCalloc(n, size, nullptr);
}

void* MallocZoneValloc(malloc_zone_t* zone, size_t size) {
  return ShimValloc(size, nullptr);
}

void MallocZoneFree(malloc_zone_t* zone, void* ptr) {
  return ShimFree(ptr, nullptr);
}

void* MallocZoneRealloc(malloc_zone_t* zone, void* ptr, size_t size) {
  return ShimRealloc(ptr, size, nullptr);
}

void MallocZoneDestroy(malloc_zone_t* zone) {
  // No support to destroy the zone for now.
}

void* MallocZoneMemalign(malloc_zone_t* zone, size_t alignment, size_t size) {
  return ShimMemalign(alignment, size, nullptr);
}

void MallocZoneFreeDefiniteSize(malloc_zone_t* zone, void* ptr, size_t size) {
  return ShimFree(ptr, nullptr);
}

malloc_introspection_t g_mac_malloc_introspection{};
malloc_zone_t g_mac_malloc_zone{};

// Replaces the default malloc zone with our own malloc zone backed by
// PartitionAlloc.  Since we'd like to make as much code as possible to use our
// own memory allocator (and reduce bugs caused by mixed use of the system
// allocator and our own allocator), run the following function
// `InitializeDefaultAllocatorPartitionRoot` with the highest priority.
//
// Note that, despite of the highest priority of the initialization order,
// [NSThread init] runs before InitializeDefaultMallocZoneWithPartitionAlloc
// unfortunately and allocates memory with the system allocator.  Plus, the
// allocated memory will be deallocated with the default zone's `free` at that
// moment without using a zone dispatcher.  Hence, our own `free` function
// receives an address allocated by the system allocator.
__attribute__((constructor(0))) void
InitializeDefaultMallocZoneWithPartitionAlloc() {
  // Instantiate the existing regular and purgeable zones in order to make the
  // existing purgeable zone use the existing regular zone since PartitionAlloc
  // doesn't support a purgeable zone.
  ignore_result(malloc_default_zone());
  ignore_result(malloc_default_purgeable_zone());

  // Initialize the default allocator's PartitionRoot with the existing zone.
  InitializeDefaultAllocatorPartitionRoot();

  // Create our own malloc zone.
  g_mac_malloc_introspection.enumerator = MallocIntrospectionEnumerator;
  g_mac_malloc_introspection.good_size = MallocIntrospectionGoodSize;
  g_mac_malloc_introspection.check = MallocIntrospectionCheck;
  g_mac_malloc_introspection.print = MallocIntrospectionPrint;
  g_mac_malloc_introspection.log = MallocIntrospectionLog;
  g_mac_malloc_introspection.force_lock = MallocIntrospectionForceLock;
  g_mac_malloc_introspection.force_unlock = MallocIntrospectionForceUnlock;
  g_mac_malloc_introspection.statistics = MallocIntrospectionStatistics;
  g_mac_malloc_introspection.zone_locked = MallocIntrospectionZoneLocked;
  g_mac_malloc_introspection.enable_discharge_checking =
      MallocIntrospectionEnableDischargeChecking;
  g_mac_malloc_introspection.disable_discharge_checking =
      MallocIntrospectionDisableDischargeChecking;
  g_mac_malloc_introspection.discharge = MallocIntrospectionDischarge;
  g_mac_malloc_introspection.enumerate_discharged_pointers =
      MallocIntrospectionEnumerateDischargedPointers;
  g_mac_malloc_introspection.reinit_lock = MallocIntrospectionReinitLock;
  g_mac_malloc_introspection.print_task = MallocIntrospectionPrintTask;
  g_mac_malloc_introspection.task_statistics =
      MallocIntrospectionTaskStatistics;
  // `version` member indicates which APIs are supported in this zone.
  //   version >= 5: memalign is supported
  //   version >= 6: free_definite_size is supported
  //   version >= 7: introspect's discharge family is supported
  //   version >= 8: pressure_relief is supported
  //   version >= 9: introspect.reinit_lock is supported
  //   version >= 10: claimed_address is supported
  //   version >= 11: introspect.print_task is supported
  //   version >= 12: introspect.task_statistics is supported
  g_mac_malloc_zone.version = 9;
  g_mac_malloc_zone.zone_name = "PartitionAlloc";
  g_mac_malloc_zone.introspect = &g_mac_malloc_introspection;
  g_mac_malloc_zone.size = MallocZoneSize;
  g_mac_malloc_zone.malloc = MallocZoneMalloc;
  g_mac_malloc_zone.calloc = MallocZoneCalloc;
  g_mac_malloc_zone.valloc = MallocZoneValloc;
  g_mac_malloc_zone.free = MallocZoneFree;
  g_mac_malloc_zone.realloc = MallocZoneRealloc;
  g_mac_malloc_zone.destroy = MallocZoneDestroy;
  g_mac_malloc_zone.batch_malloc = nullptr;
  g_mac_malloc_zone.batch_free = nullptr;
  g_mac_malloc_zone.memalign = MallocZoneMemalign;
  g_mac_malloc_zone.free_definite_size = MallocZoneFreeDefiniteSize;
  g_mac_malloc_zone.pressure_relief = nullptr;
  g_mac_malloc_zone.claimed_address = nullptr;

  // Install our own malloc zone.
  malloc_zone_register(&g_mac_malloc_zone);

  // Make our own zone the default zone.
  for (unsigned int retry_count = 0;; ++retry_count) {
    vm_address_t* zones = nullptr;
    unsigned int zone_count = 0;
    kern_return_t result =
        malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zone_count);
    MACH_CHECK(result == KERN_SUCCESS, result) << "malloc_get_all_zones";

    malloc_zone_t* top_zone = reinterpret_cast<malloc_zone_t*>(zones[0]);
    if (top_zone == &g_mac_malloc_zone) {
      break;  // Our own malloc zone is now the default zone.
    }
    CHECK_LE(retry_count, zone_count);

    // Reorder malloc zones so that our own zone becomes the default one.
    malloc_zone_unregister(top_zone);
    malloc_zone_register(top_zone);
  }
}

}  // namespace

}  // namespace allocator
}  // namespace base
