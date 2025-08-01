// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/reservation_offset_table.h"

#include "partition_alloc/buildflags.h"

namespace partition_alloc::internal {

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
PA_CONSTINIT ReservationOffsetTable::_ReservationOffsetTable<
    ReservationOffsetTable::kRegularOffsetTableLength>
    ReservationOffsetTable::regular_pool_table_;
PA_CONSTINIT ReservationOffsetTable::_ReservationOffsetTable<
    ReservationOffsetTable::kBRPOffsetTableLength>
    ReservationOffsetTable::brp_pool_table_;
PA_CONSTINIT ReservationOffsetTable::_ReservationOffsetTable<
    ReservationOffsetTable::kConfigurableOffsetTableLength>
    ReservationOffsetTable::configurable_pool_table_;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
// If thread isolation support is enabled, we need to write-protect the tables
// of the thread isolated pool. For this, the thread isolated ones start on a
// page boundary.
PA_THREAD_ISOLATED_ALIGN
PA_CONSTINIT ReservationOffsetTable::_ReservationOffsetTable<
    ReservationOffsetTable::kThreadIsolatedOffsetTableLength,
    ReservationOffsetTable::kThreadIsolatedOffsetTablePaddingSize>
    ReservationOffsetTable::thread_isolated_pool_table_;
#endif
#else
// A single table for the entire 32-bit address space.
PA_CONSTINIT ReservationOffsetTable::_ReservationOffsetTable<
    ReservationOffsetTable::kReservationOffsetTableLength>
    ReservationOffsetTable::reservation_offset_table_;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
