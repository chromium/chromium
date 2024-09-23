// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains all the logic necessary to intercept allocations on
// macOS. "malloc zones" are an abstraction that allows the process to intercept
// all malloc-related functions.  There is no good mechanism [short of
// interposition] to determine new malloc zones are added, so there's no clean
// mechanism to intercept all malloc zones. This file contains logic to
// intercept the default and purgeable zones, which always exist. A cursory
// review of Chrome seems to imply that non-default zones are almost never used.
//
// This file also contains logic to intercept Core Foundation and Objective-C
// allocations. The implementations forward to the default malloc zone, so the
// only reason to intercept these calls is to re-label OOM crashes with slightly
// more details.

#include "partition_alloc/shim/allocator_interception_apple.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <mach/mach.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <new>

#include "partition_alloc/build_config.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/partition_alloc_base/apple/mach_logging.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/shim/malloc_zone_functions_apple.h"
#include "partition_alloc/third_party/apple_apsl/CFBase.h"

#if PA_BUILDFLAG(IS_IOS)
#include "partition_alloc/partition_alloc_base/ios/ios_util.h"
#else
#include "partition_alloc/partition_alloc_base/mac/mac_util.h"
#endif

// The patching of Objective-C runtime bits must be done without any
// interference from the ARC machinery.
#if PA_HAS_FEATURE(objc_arc)
#error "This file must not be compiled with ARC."
#endif

namespace allocator_shim {

bool g_replaced_default_zone = false;

namespace {

bool g_oom_killer_enabled;
bool g_allocator_shims_failed_to_install;

// Starting with Mac OS X 10.7, the zone allocators set up by the system are
// read-only, to prevent them from being overwritten in an attack. However,
// blindly unprotecting and reprotecting the zone allocators fails with
// GuardMalloc because GuardMalloc sets up its zone allocator using a block of
// memory in its bss. Explicit saving/restoring of the protection is required.
//
// This function takes a pointer to a malloc zone, de-protects it if necessary,
// and returns (in the out parameters) a region of memory (if any) to be
// re-protected when modifications are complete. This approach assumes that
// there is no contention for the protection of this memory.
//
// Returns true if the malloc zone was properly de-protected, or false
// otherwise. If this function returns false, the out parameters are invalid and
// the region does not need to be re-protected.
bool DeprotectMallocZone(ChromeMallocZone* default_zone,
                         vm_address_t* reprotection_start,
                         vm_size_t* reprotection_length,
                         vm_prot_t* reprotection_value) {
  mach_port_t unused;
  *reprotection_start = reinterpret_cast<vm_address_t>(default_zone);
  struct vm_region_basic_info_64 info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  kern_return_t result =
      vm_region_64(mach_task_self(), reprotection_start, reprotection_length,
                   VM_REGION_BASIC_INFO_64,
                   reinterpret_cast<vm_region_info_t>(&info), &count, &unused);
  if (result != KERN_SUCCESS) {
    PA_MACH_LOG(ERROR, result) << "vm_region_64";
    return false;
  }

  // The kernel always returns a null object for VM_REGION_BASIC_INFO_64, but
  // balance it with a deallocate in case this ever changes. See
  // the VM_REGION_BASIC_INFO_64 case in vm_map_region() in 10.15's
  // https://opensource.apple.com/source/xnu/xnu-6153.11.26/osfmk/vm/vm_map.c .
  mach_port_deallocate(mach_task_self(), unused);

  if (!(info.max_protection & VM_PROT_WRITE)) {
    PA_LOG(ERROR) << "Invalid max_protection " << info.max_protection;
    return false;
  }

  // Does the region fully enclose the zone pointers? Possibly unwarranted
  // simplification used: using the size of a full version 10 malloc zone rather
  // than the actual smaller size if the passed-in zone is not version 10.
  PA_DCHECK(*reprotection_start <=
            reinterpret_cast<vm_address_t>(default_zone));
  vm_size_t zone_offset = reinterpret_cast<vm_address_t>(default_zone) -
                          reinterpret_cast<vm_address_t>(*reprotection_start);
  PA_DCHECK(zone_offset + sizeof(ChromeMallocZone) <= *reprotection_length);

  if (info.protection & VM_PROT_WRITE) {
    // No change needed; the zone is already writable.
    *reprotection_start = 0;
    *reprotection_length = 0;
    *reprotection_value = VM_PROT_NONE;
  } else {
    *reprotection_value = info.protection;
    result =
        vm_protect(mach_task_self(), *reprotection_start, *reprotection_length,
                   false, info.protection | VM_PROT_WRITE);
    if (result != KERN_SUCCESS) {
      PA_MACH_LOG(ERROR, result) << "vm_protect";
      return false;
    }
  }
  return true;
}

#if !defined(ADDRESS_SANITIZER)

MallocZoneFunctions g_old_zone;
MallocZoneFunctions g_old_purgeable_zone;

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

void* oom_killer_malloc(struct _malloc_zone_t* zone, size_t size) {
  void* result = g_old_zone.malloc(zone, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void* oom_killer_calloc(struct _malloc_zone_t* zone,
                        size_t num_items,
                        size_t size) {
  void* result = g_old_zone.calloc(zone, num_items, size);
  if (!result && num_items && size) {
    partition_alloc::TerminateBecauseOutOfMemory(num_items * size);
  }
  return result;
}

void* oom_killer_valloc(struct _malloc_zone_t* zone, size_t size) {
  void* result = g_old_zone.valloc(zone, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void oom_killer_free(struct _malloc_zone_t* zone, void* ptr) {
  g_old_zone.free(zone, ptr);
}

void* oom_killer_realloc(struct _malloc_zone_t* zone, void* ptr, size_t size) {
  void* result = g_old_zone.realloc(zone, ptr, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void* oom_killer_memalign(struct _malloc_zone_t* zone,
                          size_t alignment,
                          size_t size) {
  void* result = g_old_zone.memalign(zone, alignment, size);
  // Only die if posix_memalign would have returned ENOMEM, since there are
  // other reasons why null might be returned. See posix_memalign() in 10.15's
  // https://opensource.apple.com/source/libmalloc/libmalloc-283/src/malloc.c .
  if (!result && size && alignment >= sizeof(void*) &&
      partition_alloc::internal::base::bits::HasSingleBit(alignment)) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

void* oom_killer_malloc_purgeable(struct _malloc_zone_t* zone, size_t size) {
  void* result = g_old_purgeable_zone.malloc(zone, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void* oom_killer_calloc_purgeable(struct _malloc_zone_t* zone,
                                  size_t num_items,
                                  size_t size) {
  void* result = g_old_purgeable_zone.calloc(zone, num_items, size);
  if (!result && num_items && size) {
    partition_alloc::TerminateBecauseOutOfMemory(num_items * size);
  }
  return result;
}

void* oom_killer_valloc_purgeable(struct _malloc_zone_t* zone, size_t size) {
  void* result = g_old_purgeable_zone.valloc(zone, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void oom_killer_free_purgeable(struct _malloc_zone_t* zone, void* ptr) {
  g_old_purgeable_zone.free(zone, ptr);
}

void* oom_killer_realloc_purgeable(struct _malloc_zone_t* zone,
                                   void* ptr,
                                   size_t size) {
  void* result = g_old_purgeable_zone.realloc(zone, ptr, size);
  if (!result && size) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

void* oom_killer_memalign_purgeable(struct _malloc_zone_t* zone,
                                    size_t alignment,
                                    size_t size) {
  void* result = g_old_purgeable_zone.memalign(zone, alignment, size);
  // Only die if posix_memalign would have returned ENOMEM, since there are
  // other reasons why null might be returned. See posix_memalign() in 10.15's
  // https://opensource.apple.com/source/libmalloc/libmalloc-283/src/malloc.c .
  if (!result && size && alignment >= sizeof(void*) &&
      partition_alloc::internal::base::bits::HasSingleBit(alignment)) {
    partition_alloc::TerminateBecauseOutOfMemory(size);
  }
  return result;
}

#endif  // !defined(ADDRESS_SANITIZER)

#if !defined(ADDRESS_SANITIZER)

// === Core Foundation CFAllocators ===

bool CanGetContextForCFAllocator() {
#if PA_BUILDFLAG(IS_IOS)
  return !partition_alloc::internal::base::ios::IsRunningOnOrLater(17, 0, 0);
#else
  // As of macOS 14, the allocators are in read-only memory and can no longer be
  // altered.
  return partition_alloc::internal::base::mac::MacOSMajorVersion() < 14;
#endif
}

CFAllocatorContext* ContextForCFAllocator(CFAllocatorRef allocator) {
  ChromeCFAllocatorLions* our_allocator = const_cast<ChromeCFAllocatorLions*>(
      reinterpret_cast<const ChromeCFAllocatorLions*>(allocator));
  return &our_allocator->_context;
}

CFAllocatorAllocateCallBack g_old_cfallocator_system_default;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc;
CFAllocatorAllocateCallBack g_old_cfallocator_malloc_zone;

void* oom_killer_cfallocator_system_default(CFIndex alloc_size,
                                            CFOptionFlags hint,
                                            void* info) {
  void* result = g_old_cfallocator_system_default(alloc_size, hint, info);
  if (!result) {
    partition_alloc::TerminateBecauseOutOfMemory(
        static_cast<size_t>(alloc_size));
  }
  return result;
}

void* oom_killer_cfallocator_malloc(CFIndex alloc_size,
                                    CFOptionFlags hint,
                                    void* info) {
  void* result = g_old_cfallocator_malloc(alloc_size, hint, info);
  if (!result) {
    partition_alloc::TerminateBecauseOutOfMemory(
        static_cast<size_t>(alloc_size));
  }
  return result;
}

void* oom_killer_cfallocator_malloc_zone(CFIndex alloc_size,
                                         CFOptionFlags hint,
                                         void* info) {
  void* result = g_old_cfallocator_malloc_zone(alloc_size, hint, info);
  if (!result) {
    partition_alloc::TerminateBecauseOutOfMemory(
        static_cast<size_t>(alloc_size));
  }
  return result;
}

#endif  // !defined(ADDRESS_SANITIZER)

// === Cocoa NSObject allocation ===

typedef id (*allocWithZone_t)(id, SEL, NSZone*);
allocWithZone_t g_old_allocWithZone;

id oom_killer_allocWithZone(id self, SEL _cmd, NSZone* zone) {
  id result = g_old_allocWithZone(self, _cmd, zone);
  if (!result) {
    partition_alloc::TerminateBecauseOutOfMemory(0);
  }
  return result;
}

void UninterceptMallocZoneForTesting(struct _malloc_zone_t* zone) {
  ChromeMallocZone* chrome_zone = reinterpret_cast<ChromeMallocZone*>(zone);
  if (!IsMallocZoneAlreadyStored(chrome_zone)) {
    return;
  }
  MallocZoneFunctions& functions = GetFunctionsForZone(zone);
  ReplaceZoneFunctions(chrome_zone, &functions);
}

}  // namespace

bool UncheckedMallocMac(size_t size, void** result) {
#if defined(ADDRESS_SANITIZER)
  *result = malloc(size);
#else
  if (g_old_zone.malloc) {
    *result = g_old_zone.malloc(malloc_default_zone(), size);
  } else {
    *result = malloc(size);
  }
#endif  // defined(ADDRESS_SANITIZER)

  return *result != NULL;
}

bool UncheckedCallocMac(size_t num_items, size_t size, void** result) {
#if defined(ADDRESS_SANITIZER)
  *result = calloc(num_items, size);
#else
  if (g_old_zone.calloc) {
    *result = g_old_zone.calloc(malloc_default_zone(), num_items, size);
  } else {
    *result = calloc(num_items, size);
  }
#endif  // defined(ADDRESS_SANITIZER)

  return *result != NULL;
}

void InitializeDefaultDispatchToMacAllocator() {
  StoreFunctionsForAllZones();
}

void StoreFunctionsForDefaultZone() {
  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  StoreMallocZone(default_zone);
}

void StoreFunctionsForAllZones() {
  // This ensures that the default zone is always at the front of the array,
  // which is important for performance.
  StoreFunctionsForDefaultZone();

  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr = malloc_get_all_zones(mach_task_self(), 0, &zones, &count);
  if (kr != KERN_SUCCESS) {
    return;
  }
  for (unsigned int i = 0; i < count; ++i) {
    ChromeMallocZone* zone = reinterpret_cast<ChromeMallocZone*>(zones[i]);
    StoreMallocZone(zone);
  }
}

void ReplaceFunctionsForStoredZones(const MallocZoneFunctions* functions) {
  // The default zone does not get returned in malloc_get_all_zones().
  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  if (DoesMallocZoneNeedReplacing(default_zone, functions)) {
    ReplaceZoneFunctions(default_zone, functions);
  }

  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr =
      malloc_get_all_zones(mach_task_self(), nullptr, &zones, &count);
  if (kr != KERN_SUCCESS) {
    return;
  }
  for (unsigned int i = 0; i < count; ++i) {
    ChromeMallocZone* zone = reinterpret_cast<ChromeMallocZone*>(zones[i]);
    if (DoesMallocZoneNeedReplacing(zone, functions)) {
      ReplaceZoneFunctions(zone, functions);
    }
  }
  g_replaced_default_zone = true;
}

void InterceptAllocationsMac() {
  if (g_oom_killer_enabled) {
    return;
  }

  g_oom_killer_enabled = true;

  // === C malloc/calloc/valloc/realloc/posix_memalign ===

  // This approach is not perfect, as requests for amounts of memory larger than
  // MALLOC_ABSOLUTE_MAX_SIZE (currently SIZE_T_MAX - (2 * PAGE_SIZE)) will
  // still fail with a NULL rather than dying (see malloc_zone_malloc() in
  // https://opensource.apple.com/source/libmalloc/libmalloc-283/src/malloc.c
  // for details). Unfortunately, it's the best we can do. Also note that this
  // does not affect allocations from non-default zones.

#if !defined(ADDRESS_SANITIZER)
  // Don't do anything special on OOM for the malloc zones replaced by
  // AddressSanitizer, as modifying or protecting them may not work correctly.
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // The malloc zone backed by PartitionAlloc crashes by default, so there is
  // no need to install the OOM killer.
  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  if (!IsMallocZoneAlreadyStored(default_zone)) {
    StoreZoneFunctions(default_zone, &g_old_zone);
    MallocZoneFunctions new_functions = {};
    new_functions.malloc = oom_killer_malloc;
    new_functions.calloc = oom_killer_calloc;
    new_functions.valloc = oom_killer_valloc;
    new_functions.free = oom_killer_free;
    new_functions.realloc = oom_killer_realloc;
    new_functions.memalign = oom_killer_memalign;

    ReplaceZoneFunctions(default_zone, &new_functions);
    g_replaced_default_zone = true;
  }
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  ChromeMallocZone* purgeable_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_purgeable_zone());
  if (purgeable_zone && !IsMallocZoneAlreadyStored(purgeable_zone)) {
    StoreZoneFunctions(purgeable_zone, &g_old_purgeable_zone);
    MallocZoneFunctions new_functions = {};
    new_functions.malloc = oom_killer_malloc_purgeable;
    new_functions.calloc = oom_killer_calloc_purgeable;
    new_functions.valloc = oom_killer_valloc_purgeable;
    new_functions.free = oom_killer_free_purgeable;
    new_functions.realloc = oom_killer_realloc_purgeable;
    new_functions.memalign = oom_killer_memalign_purgeable;
    ReplaceZoneFunctions(purgeable_zone, &new_functions);
  }
#endif

  // === C malloc_zone_batch_malloc ===

  // batch_malloc is omitted because the default malloc zone's implementation
  // only supports batch_malloc for "tiny" allocations from the free list. It
  // will fail for allocations larger than "tiny", and will only allocate as
  // many blocks as it's able to from the free list. These factors mean that it
  // can return less than the requested memory even in a non-out-of-memory
  // situation. There's no good way to detect whether a batch_malloc failure is
  // due to these other factors, or due to genuine memory or address space
  // exhaustion. The fact that it only allocates space from the "tiny" free list
  // means that it's likely that a failure will not be due to memory exhaustion.
  // Similarly, these constraints on batch_malloc mean that callers must always
  // be expecting to receive less memory than was requested, even in situations
  // where memory pressure is not a concern. Finally, the only public interface
  // to batch_malloc is malloc_zone_batch_malloc, which is specific to the
  // system's malloc implementation. It's unlikely that anyone's even heard of
  // it.

#ifndef ADDRESS_SANITIZER
  // === Core Foundation CFAllocators ===

  // This will not catch allocation done by custom allocators, but will catch
  // all allocation done by system-provided ones.

  PA_CHECK(!g_old_cfallocator_system_default && !g_old_cfallocator_malloc &&
           !g_old_cfallocator_malloc_zone)
      << "Old allocators unexpectedly non-null";

  bool cf_allocator_internals_known = CanGetContextForCFAllocator();

  if (cf_allocator_internals_known) {
    CFAllocatorContext* context =
        ContextForCFAllocator(kCFAllocatorSystemDefault);
    PA_CHECK(context) << "Failed to get context for kCFAllocatorSystemDefault.";
    g_old_cfallocator_system_default = context->allocate;
    PA_CHECK(g_old_cfallocator_system_default)
        << "Failed to get kCFAllocatorSystemDefault allocation function.";
    context->allocate = oom_killer_cfallocator_system_default;

    context = ContextForCFAllocator(kCFAllocatorMalloc);
    PA_CHECK(context) << "Failed to get context for kCFAllocatorMalloc.";
    g_old_cfallocator_malloc = context->allocate;
    PA_CHECK(g_old_cfallocator_malloc)
        << "Failed to get kCFAllocatorMalloc allocation function.";
    context->allocate = oom_killer_cfallocator_malloc;

    context = ContextForCFAllocator(kCFAllocatorMallocZone);
    PA_CHECK(context) << "Failed to get context for kCFAllocatorMallocZone.";
    g_old_cfallocator_malloc_zone = context->allocate;
    PA_CHECK(g_old_cfallocator_malloc_zone)
        << "Failed to get kCFAllocatorMallocZone allocation function.";
    context->allocate = oom_killer_cfallocator_malloc_zone;
  }
#endif

  // === Cocoa NSObject allocation ===

  // Note that both +[NSObject new] and +[NSObject alloc] call through to
  // +[NSObject allocWithZone:].

  PA_CHECK(!g_old_allocWithZone) << "Old allocator unexpectedly non-null";

  Class nsobject_class = [NSObject class];
  Method orig_method =
      class_getClassMethod(nsobject_class, @selector(allocWithZone:));
  g_old_allocWithZone =
      reinterpret_cast<allocWithZone_t>(method_getImplementation(orig_method));
  PA_CHECK(g_old_allocWithZone)
      << "Failed to get allocWithZone allocation function.";
  method_setImplementation(orig_method,
                           reinterpret_cast<IMP>(oom_killer_allocWithZone));
}

void UninterceptMallocZonesForTesting() {
  UninterceptMallocZoneForTesting(malloc_default_zone());  // IN-TEST
  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr = malloc_get_all_zones(mach_task_self(), 0, &zones, &count);
  PA_CHECK(kr == KERN_SUCCESS);
  for (unsigned int i = 0; i < count; ++i) {
    UninterceptMallocZoneForTesting(  // IN-TEST
        reinterpret_cast<struct _malloc_zone_t*>(zones[i]));
  }

  ClearAllMallocZonesForTesting();  // IN-TEST
}

bool AreMallocZonesIntercepted() {
  return !g_allocator_shims_failed_to_install;
}

void ShimNewMallocZones() {
  StoreFunctionsForAllZones();

  // Use the functions for the default zone as a template to replace those
  // new zones.
  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  PA_DCHECK(IsMallocZoneAlreadyStored(default_zone));

  MallocZoneFunctions new_functions;
  StoreZoneFunctions(default_zone, &new_functions);
  ReplaceFunctionsForStoredZones(&new_functions);
}

void ReplaceZoneFunctions(ChromeMallocZone* zone,
                          const MallocZoneFunctions* functions) {
  // Remove protection.
  vm_address_t reprotection_start = 0;
  vm_size_t reprotection_length = 0;
  vm_prot_t reprotection_value = VM_PROT_NONE;
  bool success = DeprotectMallocZone(zone, &reprotection_start,
                                     &reprotection_length, &reprotection_value);
  if (!success) {
    g_allocator_shims_failed_to_install = true;
    return;
  }

  PA_CHECK(functions->malloc && functions->calloc && functions->valloc &&
           functions->free && functions->realloc);
  zone->malloc = functions->malloc;
  zone->calloc = functions->calloc;
  zone->valloc = functions->valloc;
  zone->free = functions->free;
  zone->realloc = functions->realloc;
  if (functions->batch_malloc) {
    zone->batch_malloc = functions->batch_malloc;
  }
  if (functions->batch_free) {
    zone->batch_free = functions->batch_free;
  }
  if (functions->size) {
    zone->size = functions->size;
  }
  if (zone->version >= 5 && functions->memalign) {
    zone->memalign = functions->memalign;
  }
  if (zone->version >= 6 && functions->free_definite_size) {
    zone->free_definite_size = functions->free_definite_size;
  }
  if (zone->version >= 10 && functions->claimed_address) {
    zone->claimed_address = functions->claimed_address;
  }
  if (zone->version >= 13 && functions->try_free_default) {
    zone->try_free_default = functions->try_free_default;
  }

  // Cap the version to the max supported to ensure malloc doesn't try to call
  // functions that weren't replaced.
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 130000) || \
    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160100)
  zone->version = std::min(zone->version, 13U);
#else
  zone->version = std::min(zone->version, 12U);
#endif

  // Restore protection if it was active.
  if (reprotection_start) {
    kern_return_t result =
        vm_protect(mach_task_self(), reprotection_start, reprotection_length,
                   false, reprotection_value);
    PA_MACH_DCHECK(result == KERN_SUCCESS, result) << "vm_protect";
  }
}

}  // namespace allocator_shim

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
