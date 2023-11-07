// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/reservation_offset_table.h"

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"

namespace partition_alloc::internal {

#if BUILDFLAG(HAS_64_BIT_POINTERS)
ReservationOffsetTable ReservationOffsetTable::singleton_;
#else
ReservationOffsetTable::_ReservationOffsetTable
    ReservationOffsetTable::reservation_offset_table_;
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
