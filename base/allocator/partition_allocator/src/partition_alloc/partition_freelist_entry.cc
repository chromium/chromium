// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_freelist_entry.h"

#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_check.h"

namespace partition_alloc::internal {

void FreelistCorruptionDetected(size_t slot_size) {
  // Make it visible in minidumps.
  PA_DEBUG_DATA_ON_STACK("slotsize", slot_size);
  PA_IMMEDIATE_CRASH();
}

}  // namespace partition_alloc::internal
