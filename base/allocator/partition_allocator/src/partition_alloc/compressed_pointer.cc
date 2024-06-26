// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/compressed_pointer.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)

namespace partition_alloc::internal {

// We keep the useful part in |g_base_| as 1s to speed up decompression.
alignas(kPartitionCachelineSize)
    PA_COMPONENT_EXPORT(PARTITION_ALLOC) CompressedPointerBaseGlobal::Base
    CompressedPointerBaseGlobal::g_base_ = {.base = kUsefulBitsMask};

void CompressedPointerBaseGlobal::SetBase(uintptr_t base) {
  PA_DCHECK(!IsSet());
  PA_DCHECK((base & kUsefulBitsMask) == 0);
  g_base_.base = base | kUsefulBitsMask;
}

void CompressedPointerBaseGlobal::ResetBaseForTesting() {
  g_base_.base = kUsefulBitsMask;
}

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
