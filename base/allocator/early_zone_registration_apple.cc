// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/early_zone_registration_apple.h"

#include <mach/mach.h>
#include <malloc/malloc.h>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/shim/early_zone_registration_constants.h"

// BASE_EXPORT tends to be defined as soon as anything from //base is included.
#if defined(BASE_EXPORT)
#error "This file cannot depend on //base"
#endif

namespace partition_alloc {

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

void EarlyMallocZoneRegistration() {}
void AllowDoublePartitionAllocZoneRegistration() {}

#else

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. See also in
// chrome_exe_main_mac.cc.
void abort_report_np(const char* fmt, ...);
}

namespace {

malloc_zone_t* GetDefaultMallocZone() {
  // malloc_default_zone() does not return... the default zone, but the
  // initial one. The default one is the first element of the default zone
  // array.
  unsigned int zone_count = 0;
  vm_address_t* zones = nullptr;
  kern_return_t result = malloc_get_all_zones(
      mach_task_self(), /*reader=*/nullptr, &zones, &zone_count);
  if (result != KERN_SUCCESS) {
    abort_report_np("Cannot enumerate malloc() zones");
  }
  return reinterpret_cast<malloc_zone_t*>(zones[0]);
}

}  // namespace

void EarlyMallocZoneRegistration() {
  // Must have static storage duration, as raw pointers are passed to
  // libsystem_malloc.
  static malloc_zone_t g_delegating_zone;
  static malloc_introspection_t g_delegating_zone_introspect;
  static malloc_zone_t* g_default_zone;

  // Make sure that the default zone is instantiated.
  malloc_zone_t* purgeable_zone = malloc_default_purgeable_zone();

  g_default_zone = GetDefaultMallocZone();

  // The delegating zone:
  // - Forwards all allocations to the existing default zone
  // - Does *not* claim to own any memory, meaning that it will always be
  //   skipped in free() in libsystem_malloc.dylib.
  //
  // This is a temporary zone, until it gets replaced by PartitionAlloc, inside
  // the main library. Since the main library depends on many external
  // libraries, we cannot install PartitionAlloc as the default zone without
  // concurrency issues.
  //
  // Instead, what we do is here, while the process is single-threaded:
  // - Register the delegating zone as the default one.
  // - Set the original (libsystem_malloc's) one as the second zone
  //
  // Later, when PartitionAlloc initializes, we replace the default (delegating)
  // zone with ours. The end state is:
  // 1. PartitionAlloc zone
  // 2. libsystem_malloc zone

  // Set up of the delegating zone. Note that it doesn't just forward calls to
  // the default zone. This is because the system zone's malloc_zone_t pointer
  // actually points to a larger struct, containing allocator metadata. So if we
  // pass as the first parameter the "simple" delegating zone pointer, then we
  // immediately crash inside the system zone functions. So we need to replace
  // the zone pointer as well.
  //
  // Calls fall into 4 categories:
  // - Allocation calls: forwarded to the real system zone
  // - "Is this pointer yours" calls: always answer no
  // - free(): Should never be called, but is in practice, see comments below.
  // - Diagnostics and debugging: these are typically called for every
  //   zone. They are no-ops for us, as we don't want to double-count, or lock
  //   the data structures of the real zone twice.

  // Allocation: Forward to the real zone.
  g_delegating_zone.malloc = [](malloc_zone_t* zone, size_t size) {
    return g_default_zone->malloc(g_default_zone, size);
  };
  g_delegating_zone.calloc = [](malloc_zone_t* zone, size_t num_items,
                                size_t size) {
    return g_default_zone->calloc(g_default_zone, num_items, size);
  };
  g_delegating_zone.valloc = [](malloc_zone_t* zone, size_t size) {
    return g_default_zone->valloc(g_default_zone, size);
  };
  g_delegating_zone.realloc = [](malloc_zone_t* zone, void* ptr, size_t size) {
    return g_default_zone->realloc(g_default_zone, ptr, size);
  };
  g_delegating_zone.batch_malloc = [](malloc_zone_t* zone, size_t size,
                                      void** results, unsigned num_requested) {
    return g_default_zone->batch_malloc(g_default_zone, size, results,
                                        num_requested);
  };
  g_delegating_zone.memalign = [](malloc_zone_t* zone, size_t alignment,
                                  size_t size) {
    return g_default_zone->memalign(g_default_zone, alignment, size);
  };

  // Does ptr belong to this zone? Return value is != 0 if so.
  g_delegating_zone.size = [](malloc_zone_t* zone, const void* ptr) -> size_t {
    return 0;
  };

  // Free functions.
  // The normal path for freeing memory is:
  // 1. Try all zones in order, call zone->size(ptr)
  // 2. If zone->size(ptr) != 0, call zone->free(ptr) (or free_definite_size)
  // 3. If no zone matches, crash.
  //
  // Since this zone always returns 0 in size() (see above), then zone->free()
  // should never be called. Unfortunately, this is not the case, as some places
  // in CoreFoundation call malloc_zone_free(zone, ptr) directly. So rather than
  // crashing, forward the call. It's the caller's responsibility to use the
  // same zone for free() as for the allocation (this is in the contract of
  // malloc_zone_free()).
  //
  // However, note that the sequence of calls size() -> free() is not possible
  // for this zone, as size() always returns 0.
  g_delegating_zone.free = [](malloc_zone_t* zone, void* ptr) {
    return g_default_zone->free(g_default_zone, ptr);
  };
  g_delegating_zone.free_definite_size = [](malloc_zone_t* zone, void* ptr,
                                            size_t size) {
    return g_default_zone->free_definite_size(g_default_zone, ptr, size);
  };
  g_delegating_zone.batch_free = [](malloc_zone_t* zone, void** to_be_freed,
                                    unsigned num_to_be_freed) {
    return g_default_zone->batch_free(g_default_zone, to_be_freed,
                                      num_to_be_freed);
  };
#if PA_TRY_FREE_DEFAULT_IS_AVAILABLE
  g_delegating_zone.try_free_default = [](malloc_zone_t* zone, void* ptr) {
    return g_default_zone->try_free_default(g_default_zone, ptr);
  };
#endif

  // Diagnostics and debugging.
  //
  // Do nothing to reduce memory footprint, the real
  // zone will do it.
  g_delegating_zone.pressure_relief = [](malloc_zone_t* zone,
                                         size_t goal) -> size_t { return 0; };

  // Introspection calls are not all optional, for instance locking and
  // unlocking before/after fork() is not optional.
  //
  // Nothing to enumerate.
  g_delegating_zone_introspect.enumerator =
      [](task_t task, void*, unsigned type_mask, vm_address_t zone_address,
         memory_reader_t reader,
         vm_range_recorder_t recorder) -> kern_return_t {
    return KERN_SUCCESS;
  };
  // Need to provide a real implementation, it is used for e.g. array sizing.
  g_delegating_zone_introspect.good_size = [](malloc_zone_t* zone,
                                              size_t size) {
    return g_default_zone->introspect->good_size(g_default_zone, size);
  };
  // Nothing to do.
  g_delegating_zone_introspect.check = [](malloc_zone_t* zone) -> boolean_t {
    return true;
  };
  g_delegating_zone_introspect.print = [](malloc_zone_t* zone,
                                          boolean_t verbose) {};
  g_delegating_zone_introspect.log = [](malloc_zone_t*, void*) {};
  // Do not forward the lock / unlock calls. Since the default zone is still
  // there, we should not lock here, as it would lock the zone twice (all
  // zones are locked before fork().). Rather, do nothing, since this fake
  // zone does not need any locking.
  g_delegating_zone_introspect.force_lock = [](malloc_zone_t* zone) {};
  g_delegating_zone_introspect.force_unlock = [](malloc_zone_t* zone) {};
  g_delegating_zone_introspect.reinit_lock = [](malloc_zone_t* zone) {};
  // No stats.
  g_delegating_zone_introspect.statistics = [](malloc_zone_t* zone,
                                               malloc_statistics_t* stats) {};
  // We are not locked.
  g_delegating_zone_introspect.zone_locked =
      [](malloc_zone_t* zone) -> boolean_t { return false; };
  // Don't support discharge checking.
  g_delegating_zone_introspect.enable_discharge_checking =
      [](malloc_zone_t* zone) -> boolean_t { return false; };
  g_delegating_zone_introspect.disable_discharge_checking =
      [](malloc_zone_t* zone) {};
  g_delegating_zone_introspect.discharge = [](malloc_zone_t* zone,
                                              void* memory) {};

  // Could use something lower to support fewer functions, but this is
  // consistent with the real zone installed by PartitionAlloc.
  g_delegating_zone.version = allocator_shim::kZoneVersion;
  g_delegating_zone.introspect = &g_delegating_zone_introspect;
  // This name is used in PartitionAlloc's initialization to determine whether
  // it should replace the delegating zone.
  g_delegating_zone.zone_name = allocator_shim::kDelegatingZoneName;

  // Register puts the new zone at the end, unregister swaps the new zone with
  // the last one.
  // The zone array is, after these lines, in order:
  // 1. |g_default_zone|...|g_delegating_zone|
  // 2. |g_delegating_zone|...|  (no more default)
  // 3. |g_delegating_zone|...|g_default_zone|
  malloc_zone_register(&g_delegating_zone);
  malloc_zone_unregister(g_default_zone);
  malloc_zone_register(g_default_zone);

  // Make sure that the purgeable zone is after the default one.
  // Will make g_default_zone take the purgeable zone spot
  malloc_zone_unregister(purgeable_zone);
  // Add back the purgeable zone as the last one.
  malloc_zone_register(purgeable_zone);

  // Final configuration:
  // |g_delegating_zone|...|g_default_zone|purgeable_zone|

  // Sanity check.
  if (GetDefaultMallocZone() != &g_delegating_zone) {
    abort_report_np("Failed to install the delegating zone as default.");
  }
}

void AllowDoublePartitionAllocZoneRegistration() {
  unsigned int zone_count = 0;
  vm_address_t* zones = nullptr;
  kern_return_t result = malloc_get_all_zones(
      mach_task_self(), /*reader=*/nullptr, &zones, &zone_count);
  if (result != KERN_SUCCESS) {
    abort_report_np("Cannot enumerate malloc() zones");
  }

  // If PartitionAlloc is one of the zones, *change* its name so that
  // registration can happen multiple times. This works because zone
  // registration only keeps a pointer to the struct, it does not copy the data.
  for (unsigned int i = 0; i < zone_count; i++) {
    malloc_zone_t* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
    if (zone->zone_name &&
        strcmp(zone->zone_name, allocator_shim::kPartitionAllocZoneName) == 0) {
      zone->zone_name = "RenamedPartitionAlloc";
      break;
    }
  }
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}  // namespace partition_alloc
