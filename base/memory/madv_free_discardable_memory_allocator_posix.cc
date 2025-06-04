// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/madv_free_discardable_memory_allocator_posix.h"

#include <inttypes.h>
#include <sys/mman.h>

#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/tracing_buildflags.h"

namespace base {

MadvFreeDiscardableMemoryAllocatorPosix::
    MadvFreeDiscardableMemoryAllocatorPosix() {
  // Don't register dump provider if
  // SingleThreadTaskRunner::CurrentDefaultHAndle is not set, such as in tests
  // and Android Webview.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "MadvFreeDiscardableMemoryAllocator",
        SingleThreadTaskRunner::GetCurrentDefault());
  }
}

MadvFreeDiscardableMemoryAllocatorPosix::
    ~MadvFreeDiscardableMemoryAllocatorPosix() {
  trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(this);
}

std::unique_ptr<DiscardableMemory>
MadvFreeDiscardableMemoryAllocatorPosix::AllocateLockedDiscardableMemory(
    size_t size) {
  return std::make_unique<MadvFreeDiscardableMemoryPosix>(size,
                                                          &bytes_allocated_);
}

size_t MadvFreeDiscardableMemoryAllocatorPosix::GetBytesAllocated() const {
  return bytes_allocated_;
}

bool MadvFreeDiscardableMemoryAllocatorPosix::OnMemoryDump(
    const trace_event::MemoryDumpArgs& args,
    trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    return true;
  }

  base::trace_event::MemoryAllocatorDump* total_dump =
      pmd->CreateAllocatorDump("discardable/madv_free_allocated");
  total_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        GetBytesAllocated());
  return true;
}

}  // namespace base
