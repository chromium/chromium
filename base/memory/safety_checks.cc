// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/safety_checks.h"

namespace base {

void CheckHeapIntegrity(const void* ptr) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partition_alloc::PartitionRoot::CheckMetadataIntegrity(ptr);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace base
