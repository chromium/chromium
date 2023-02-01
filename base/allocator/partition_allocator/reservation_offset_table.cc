// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/reservation_offset_table.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

namespace partition_alloc::internal {

#if BUILDFLAG(HAS_64_BIT_POINTERS)
ReservationOffsetTable::_PaddedReservationOffsetTables
    ReservationOffsetTable::padded_reservation_offset_tables_ PA_PKEY_ALIGN;
#else
ReservationOffsetTable::_ReservationOffsetTable
    ReservationOffsetTable::reservation_offset_table_;
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
