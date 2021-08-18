// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/reservation_offset_table.h"

namespace base {
namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)
ReservationOffsetTable::_ReservationOffsetTable
    ReservationOffsetTable::reservation_offset_tables_[];
#else
ReservationOffsetTable::_ReservationOffsetTable
    ReservationOffsetTable::reservation_offset_table_;
#endif

}  // namespace internal
}  // namespace base
