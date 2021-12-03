// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_MAC_H_
#define BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_MAC_H_

// This is an Apple-only file, used to register PartitionAlloc's zone *before*
// the process becomes multi-threaded.

namespace partition_alloc {

static constexpr char kDelegatingZoneName[] =
    "DelegatingDefaultZoneForPartitionAlloc";

// Zone version. Determines which callbacks are set in the various malloc_zone_t
// structs.
constexpr int kZoneVersion = 9;

// Must be called *once*, *before* the process becomes multi-threaded.
void EarlyMallocZoneRegistration();

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_EARLY_ZONE_REGISTRATION_H_
