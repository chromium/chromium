// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SLOT_ADDRESS_AND_SIZE_H_
#define PARTITION_ALLOC_SLOT_ADDRESS_AND_SIZE_H_

// Convenience struct that lets callers easily see the bounds of an
// allocation. This is used to implement the functions in
// `bounds_checks`.

#include <cstdint>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/slot_start.h"

namespace partition_alloc {

struct PA_COMPONENT_EXPORT(PARTITION_ALLOC) SlotAddressAndSize {
  internal::UntaggedSlotStart slot_start = {};
  size_t size = 0u;

  // Gets the start address and size of the allocated slot. The input |address|
  // can point anywhere in the slot, including the slot start as well as
  // immediately past the slot.
  //
  // This isn't a general purpose function, it is used specifically for
  // obtaining BackupRefPtr's in-slot metadata. The caller is responsible for
  // ensuring that the in-slot metadata is in place for this allocation.
  static SlotAddressAndSize From(uintptr_t address, internal::pool_handle pool);

  // Terse BRP-specific version of `From()`. Caller must ensure that
  // `address` lies in the BRP pool.
  PA_ALWAYS_INLINE static SlotAddressAndSize FromBRPPool(uintptr_t address) {
    return From(address, internal::pool_handle::kBRPPoolHandle);
  }
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SLOT_ADDRESS_AND_SIZE_H_
