// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/reservation_offset_table.h"

namespace partition_alloc::internal {

#if PA_CONFIG(HAS_64_BITS_POINTERS)
ReservationOffsetTable::_PaddedReservationOffsetTables
    ReservationOffsetTable::padded_reservation_offset_tables_ PA_PKEY_ALIGN;
#else
ReservationOffsetTable::_ReservationOffsetTable
    ReservationOffsetTable::reservation_offset_table_;
#endif

}  // namespace partition_alloc::internal
