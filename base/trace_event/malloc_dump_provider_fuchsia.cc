// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include "base/trace_event/malloc_dump_provider_internal.h"
#include "partition_alloc/buildflags.h"

namespace base::trace_event {

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// static
// The pool: nothing else is reported.
const char MallocDumpProvider::kSuballocationTarget[] =
    "malloc/allocated_objects";
#endif

namespace internal {

MallocTotals ReportSystemAllocator(ProcessMemoryDump* pmd,
                                   MemoryDumpLevelOfDetail level_of_detail) {
  // TODO(fuchsia): Port, see https://crbug.com/706592.
  return {};
}

void AttributeAllocatedObjectsPool(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* allocated_objects_dump) {}

}  // namespace internal

}  // namespace base::trace_event
