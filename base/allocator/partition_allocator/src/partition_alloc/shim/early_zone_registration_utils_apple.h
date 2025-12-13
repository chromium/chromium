// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SHIM_EARLY_ZONE_REGISTRATION_UTILS_APPLE_H_
#define PARTITION_ALLOC_SHIM_EARLY_ZONE_REGISTRATION_UTILS_APPLE_H_

// This is an Apple-only file, used to register PartitionAlloc's zone *before*
// the process becomes multi-threaded. These constants are shared between the
// allocator shim which installs the PartitionAlloc's malloc zone and the
// application which installs the "early malloc zone" to reserve the zone slot.

#include <mach/mach.h>
#include <malloc/malloc.h>

#include <span>
#include <string_view>

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. See also in
// chrome_exe_main_mac.cc.
void abort_report_np(const char* fmt, ...);
}  // extern "C"

namespace allocator_shim {

// This function intentionally returns a `std::span` instead of `base::span`
// because this file cannot depend on the base library (or any other libraries
// except for the system libraries and standard C++ libraries).
inline std::span<malloc_zone_t*> GetMallocZonesOrDie() {
  vm_address_t* zones = nullptr;
  unsigned int zone_count = 0;
  kern_return_t result = malloc_get_all_zones(
      mach_task_self(), /*reader=*/nullptr, &zones, &zone_count);
  if (result != KERN_SUCCESS) [[unlikely]] {
    abort_report_np("Cannot enumerate malloc zones.");
  }
  // This is not guaranteed by the C++ standard, but commonly satisfied.
  // It must be safe to assume the same memory layout for arrays of two
  // different pointer types.
  static_assert(sizeof(vm_address_t) == sizeof(malloc_zone_t*));
  return std::span<malloc_zone_t*>(reinterpret_cast<malloc_zone_t**>(zones),
                                   zone_count);
}

inline malloc_zone_t* GetDefaultMallocZoneOrDie() {
  // malloc_default_zone() does not return... the default zone, but the initial
  // one. The default one is the first element of the default zone array.
  return GetMallocZonesOrDie()[0];
}

inline bool IsZoneAlreadyRegistered(std::string_view zone_name) {
  // Checking all the zones, in case someone registered their own zone on top of
  // our own.
  const std::span<malloc_zone_t*> zones = GetMallocZonesOrDie();
  for (const auto* zone : zones) {
    // Not a pointer comparison, as the zone was registered from another
    // library, the pointers don't match.
    if (zone->zone_name && zone->zone_name == zone_name) {
      return true;
    }
  }
  return false;
}

inline constexpr std::string_view kDelegatingZoneName(
    "DelegatingDefaultZoneForPartitionAlloc");
inline constexpr std::string_view kPartitionAllocZoneName("PartitionAlloc");

// Zone version. Determines which callbacks are set in the various malloc_zone_t
// structs.
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 130000) || \
    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160100)
#define PA_TRY_FREE_DEFAULT_IS_AVAILABLE 1
#endif
#if PA_TRY_FREE_DEFAULT_IS_AVAILABLE
inline constexpr int kZoneVersion = 13;
#else
inline constexpr int kZoneVersion = 9;
#endif

}  // namespace allocator_shim

#endif  // PARTITION_ALLOC_SHIM_EARLY_ZONE_REGISTRATION_UTILS_APPLE_H_
