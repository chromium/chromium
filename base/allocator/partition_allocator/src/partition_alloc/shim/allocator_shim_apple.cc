// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim.h"

#include <malloc/malloc.h>
#include <unistd.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/apple/mach_logging.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/shim/allocator_interception_apple.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

// No calls to malloc / new in this file. They would cause re-entrancy of
// the shim, which is hard to deal with. Keep this code as simple as possible
// and don't use any external C++ object here, not even //base ones. Even if
// they are safe to use today, in future they might be refactored.

#include "partition_alloc/shim/allocator_shim_functions.h"

namespace allocator_shim {

void TryFreeDefaultFallbackToFindZoneAndFree(void* ptr) {
  unsigned int zone_count = 0;
  vm_address_t* zones = nullptr;
  kern_return_t result =
      malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zone_count);
  PA_MACH_CHECK(result == KERN_SUCCESS, result) << "malloc_get_all_zones";

  // "find_zone_and_free" expected by try_free_default.
  //
  // libmalloc's zones call find_registered_zone() in case the default one
  // doesn't handle the allocation. We can't, so we try to emulate it. See the
  // implementation in libmalloc/src/malloc.c for details.
  // https://github.com/apple-oss-distributions/libmalloc/blob/main/src/malloc.c
  for (unsigned int i = 0; i < zone_count; ++i) {
    malloc_zone_t* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
    if (size_t size = zone->size(zone, ptr)) {
      if (zone->version >= 6 && zone->free_definite_size) {
        zone->free_definite_size(zone, ptr, size);
      } else {
        zone->free(zone, ptr);
      }
      return;
    }
  }

  // There must be an owner zone.
  PA_CHECK(false);
}

}  // namespace allocator_shim

#include "partition_alloc/shim/shim_alloc_functions.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Cpp symbols (new / delete) should always be routed through the shim layer
// except on Windows and macOS (except for PartitionAlloc-Everywhere) where the
// malloc intercept is deep enough that it also catches the cpp calls.
//
// In case of PartitionAlloc-Everywhere on macOS, malloc backed by
// allocator_shim::internal::PartitionMalloc crashes on OOM, and we need to
// avoid crashes in case of operator new() noexcept.  Thus, operator new()
// noexcept needs to be routed to
// allocator_shim::internal::PartitionMallocUnchecked through the shim layer.
#include "partition_alloc/shim/allocator_shim_override_cpp_symbols.h"
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_override_apple_default_zone.h"
#else  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_override_apple_symbols.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace allocator_shim {

void InitializeAllocatorShim() {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Prepares the default dispatch. After the intercepted malloc calls have
  // traversed the shim this will route them to the default malloc zone.
  InitializeDefaultDispatchToMacAllocator();

  MallocZoneFunctions functions = MallocZoneFunctionsToReplaceDefault();

  // This replaces the default malloc zone, causing calls to malloc & friends
  // from the codebase to be routed to ShimMalloc() above.
  ReplaceFunctionsForStoredZones(&functions);
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace allocator_shim

// Cross-checks.

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#error The allocator shim should not be compiled when building for memory tools.
#endif

#if (defined(__GNUC__) && defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && defined(_CPPUNWIND))
#error This code cannot be used when exceptions are turned on.
#endif
