// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <windows.h>

#include <malloc.h>
#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/trace_event/malloc_dump_provider_internal.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/buildflags.h"

namespace base::trace_event {

namespace {

internal::WinHeapInfo WinHeapInfoFromHandle(HANDLE heap_handle) {
  internal::WinHeapInfo info;
  ::HeapLock(heap_handle);
  PROCESS_HEAP_ENTRY heap_entry;
  heap_entry.lpData = nullptr;

  // HeapWalk emits a PROCESS_HEAP_REGION header before the blocks inside
  // that region; large VirtualAlloc-backed allocations have no header and
  // appear as orphan busy entries. See:
  // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-process_heap_entry
  uintptr_t last_region_start = 0;
  uintptr_t last_region_end = 0;

  while (::HeapWalk(heap_handle, &heap_entry) != FALSE) {
    const uintptr_t entry_addr = reinterpret_cast<uintptr_t>(heap_entry.lpData);
    if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
      info.allocated_size += heap_entry.cbData;
      info.block_count++;
      if (entry_addr < last_region_start || entry_addr >= last_region_end) {
        // Large allocations are returned by HeapWalk as orphan busy entries
        // outside any PROCESS_HEAP_REGION. They are always committed since
        // HeapAlloc never returns uncommitted memory.
        info.committed_size +=
            static_cast<size_t>(heap_entry.cbData) + heap_entry.cbOverhead;
      }
    } else if ((heap_entry.wFlags & PROCESS_HEAP_REGION) != 0) {
      // dwCommittedSize / dwUnCommittedSize are documented as optional and
      // reported as zero when unavailable. When their sum does not match
      // cbData, fall back to treating the full reserved range as committed
      // so the dump does not under-report.
      if (heap_entry.Region.dwCommittedSize +
              heap_entry.Region.dwUnCommittedSize ==
          heap_entry.cbData) {
        info.committed_size += heap_entry.Region.dwCommittedSize;
        info.uncommitted_size += heap_entry.Region.dwUnCommittedSize;
      } else {
        info.committed_size += heap_entry.cbData;
      }
      last_region_start = entry_addr;
      last_region_end = entry_addr + heap_entry.cbData;
    }
  }
  CHECK(::HeapUnlock(heap_handle) == TRUE);

  return info;
}

}  // namespace

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// static
// The pool, the only objects dump reported at every level of detail: the
// WinHeap one is kDetailed-only.
const char MallocDumpProvider::kSuballocationTarget[] =
    "malloc/allocated_objects";
#endif
// static
const char MallocDumpProvider::kWinHeap[] = "malloc/win_heap";
const char MallocDumpProvider::kWinHeapAllocatedObjects[] =
    "malloc/allocated_objects/win_heap";

namespace internal {

// static
WinHeapInfo WinHeapInfo::FromHandleForTesting(void* heap) {
  HANDLE heap_handle = static_cast<HANDLE>(heap);
  return WinHeapInfoFromHandle(heap_handle);
}

// Reports the WinHeap into its own dumps, whether or not PartitionAlloc is the
// malloc implementation: WinHeap backs malloc in one configuration and the CRT
// in the other, and the dumps should not depend on which.
MallocTotals ReportSystemAllocator(ProcessMemoryDump* pmd,
                                   MemoryDumpLevelOfDetail level_of_detail) {
  // This is too expensive on Windows, crbug.com/780735.
  if (level_of_detail != MemoryDumpLevelOfDetail::kDetailed) {
    return {};
  }

  // NOTE: crbug.com/665516. Unfortunately, there is no safe way to collect
  // information from secondary heaps due to limitations and racy nature of
  // this piece of WinAPI. Walk only whichever heap our CRT is using.
  auto main_heap_info =
      WinHeapInfoFromHandle(reinterpret_cast<HANDLE>(_get_heap_handle()));

  size_t virtual_size =
      main_heap_info.committed_size + main_heap_info.uncommitted_size;
  // Free blocks on the heap's free lists, block headers and alignment
  // padding.
  size_t wasted = 0;
  if (main_heap_info.committed_size >= main_heap_info.allocated_size) {
    wasted = main_heap_info.committed_size - main_heap_info.allocated_size;
  }

  // The objects allocated out of the WinHeap are accounted for under the
  // system allocator pool, and reported as suballocated from the WinHeap dump.
  // That keeps malloc/win_heap's effective size down to the part of the heap
  // which is not already accounted for by malloc/allocated_objects. Without
  // PartitionAlloc-as-malloc, the pool is the WinHeap objects dump instead, see
  // AttributeAllocatedObjectsPool().
  const char* objects_name = nullptr;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  objects_name = MallocDumpProvider::kWinHeapAllocatedObjects;
#endif
  MemoryAllocatorDump* win_heap_dump = CreateBackendDumps(
      pmd, level_of_detail, MallocDumpProvider::kWinHeap, objects_name,
      {.resident_size = main_heap_info.committed_size,
       .virtual_size = virtual_size,
       .committed_size = main_heap_info.committed_size,
       .allocated_objects_size = main_heap_info.allocated_size,
       .allocated_objects_count = main_heap_info.block_count,
       .wasted = wasted},
      ObjectsEdge::kSuballocation);

  // The wasted bytes are reported as a child of the WinHeap dump, so that
  // malloc/win_heap accounts for the whole committed heap: its allocated
  // objects, suballocated from it, plus what the heap holds but has not
  // handed out.
  if (wasted > 0) {
    MemoryAllocatorDump* win_heap_waste_dump = pmd->CreateAllocatorDump(
        win_heap_dump->absolute_name() + "/metadata_fragmentation_caches");
    win_heap_waste_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                                   MemoryAllocatorDump::kUnitsBytes, wasted);
  }

  return {
      .virtual_size = virtual_size,
      // Resident size is approximated with committed heap size. Note that it
      // is possible to do this with better accuracy on windows by intersecting
      // the working set with the virtual memory ranges occuipied by the heap.
      // It's not clear that this is worth it, as it's fairly expensive to do.
      .resident_size = main_heap_info.committed_size,
      .allocated_objects_size = main_heap_info.allocated_size,
      .allocated_objects_count = main_heap_info.block_count,
      .excluded_waste = wasted,
  };
}

// Where other providers suballocate from the pool itself, the pool is the
// WinHeap objects dump, so suballocate it from the heap like the other objects
// dumps are from their backend.
void AttributeAllocatedObjectsPool(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* allocated_objects_dump) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Only reported at kDetailed level of detail.
  if (pmd->GetAllocatorDump(MallocDumpProvider::kWinHeap)) {
    pmd->AddSuballocation(allocated_objects_dump->guid(),
                          MallocDumpProvider::kWinHeap);
  }
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace internal

}  // namespace base::trace_event
