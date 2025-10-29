// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/java_heap_dump_provider_android.h"

#include "base/android/java_runtime.h"
#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

// static
JavaHeapDumpProvider* JavaHeapDumpProvider::GetInstance() {
  return Singleton<JavaHeapDumpProvider,
                   LeakySingletonTraits<JavaHeapDumpProvider>>::get();
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current process.
bool JavaHeapDumpProvider::OnMemoryDump(const MemoryDumpArgs& args,
                                        ProcessMemoryDump* pmd) {
  // These numbers come from java.lang.Runtime stats.
  uint64_t total_heap_size = 0;
  uint64_t free_heap_size = 0;
  android::JavaRuntime::GetMemoryUsage(&total_heap_size, &free_heap_size);

  // This is the heap size, which only changes when the heap either grows or
  // shrinks right after a GC. Contrary to V8, ART does not provide access to
  // the allocated object size right after a GC (which is the only time we know
  // the size of live objects), so the allocated_objects below is not very
  // informative: it will move from some value right after GC to ~heap size when
  // the GC is triggered.
  //
  // As a consequence, the heap size metric will tend to be heavily quantized:
  // the heap starts at a given size (see the system property
  // "dalvik.vm.heapstartsize"), then can go down, or up, to a maximum of
  // "dalvik.vm.heapgrowthlimit", since Chromium typically does not request a
  // large heap (android:largeHeap in the application tag inside the manifest).
  MemoryAllocatorDump* outer_dump = pmd->CreateAllocatorDump("java_heap");
  outer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, total_heap_size);

  MemoryAllocatorDump* inner_dump =
      pmd->CreateAllocatorDump("java_heap/allocated_objects");
  inner_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes,
                        total_heap_size - free_heap_size);
  return true;
}

}  // namespace trace_event
}  // namespace base
