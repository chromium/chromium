// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_MAC_H_
#define BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_MAC_H_

// This is an Apple-only file, used to register PartitionAlloc's zone *before*
// the process becomes multi-threaded.

namespace partition_alloc {

static constexpr char kDelegatingZoneName[] =
    "DelegatingDefaultZoneForPartitionAlloc";
static constexpr char kPartitionAllocZoneName[] = "PartitionAlloc";

// Zone version. Determines which callbacks are set in the various malloc_zone_t
// structs.
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 130000) || \
    (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160100)
#define PA_TRY_FREE_DEFAULT_IS_AVAILABLE 1
#endif
#if PA_TRY_FREE_DEFAULT_IS_AVAILABLE
constexpr int kZoneVersion = 13;
#else
constexpr int kZoneVersion = 9;
#endif

// Must be called *once*, *before* the process becomes multi-threaded.
void EarlyMallocZoneRegistration();

// Tricks the registration code to believe that PartitionAlloc was not already
// registered. This allows a future library load to register PartitionAlloc's
// zone as well, rather than bailing out.
//
// This is mutually exclusive with EarlyMallocZoneRegistation(), and should
// ideally be removed. Indeed, by allowing two zones to be registered, we still
// end up with a split heap, and more memory usage.
//
// This is a hack for crbug.com/1274236.
void AllowDoublePartitionAllocZoneRegistration();

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_H_
