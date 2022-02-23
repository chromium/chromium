// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_

#include "base/allocator/buildflags.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MALLOC_MEMORY_TRACING_SUPPORTED
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/partition_stats.h"
#endif

namespace base {
namespace trace_event {

// Dump provider which collects process-wide memory stats.
class BASE_EXPORT MallocDumpProvider : public MemoryDumpProvider {
 public:
  // Name of the allocated_objects dump. Use this to declare suballocator dumps
  // from other dump providers.
  static const char kAllocatedObjects[];

  static MallocDumpProvider* GetInstance();

  MallocDumpProvider(const MallocDumpProvider&) = delete;
  MallocDumpProvider& operator=(const MallocDumpProvider&) = delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump* pmd) override;

  // Used by out-of-process heap-profiling. When malloc is profiled by an
  // external process, that process will be responsible for emitting metrics on
  // behalf of this one. Thus, MallocDumpProvider should not do anything.
  void EnableMetrics();
  void DisableMetrics();

 private:
  friend struct DefaultSingletonTraits<MallocDumpProvider>;

  MallocDumpProvider();
  ~MallocDumpProvider() override;

  bool emit_metrics_on_memory_dump_
      GUARDED_BY(emit_metrics_on_memory_dump_lock_) = true;
  base::Lock emit_metrics_on_memory_dump_lock_;

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // To be accurate, this requires the dump provider to be created very early,
  // which is the case. The alternative would be to drop the first data point,
  // which is not desirable as early process activity is highly relevant.
  base::TimeTicks last_memory_dump_time_ = base::TimeTicks::Now();
  uint64_t last_syscall_count_ = 0;
#endif
};

#if BUILDFLAG(USE_PARTITION_ALLOC)
// This class is used to invert the dependency of PartitionAlloc on the
// PartitionAllocMemoryDumpProvider. This implements an interface that will
// be called with memory statistics for each bucket in the allocator.
class BASE_EXPORT MemoryDumpPartitionStatsDumper final
    : public base::PartitionStatsDumper {
 public:
  MemoryDumpPartitionStatsDumper(const char* root_name,
                                 ProcessMemoryDump* memory_dump,
                                 MemoryDumpLevelOfDetail level_of_detail)
      : root_name_(root_name),
        memory_dump_(memory_dump),
        detailed_(level_of_detail != MemoryDumpLevelOfDetail::BACKGROUND) {}

  static const char* kPartitionsDumpName;

  // PartitionStatsDumper implementation.
  void PartitionDumpTotals(const char* partition_name,
                           const base::PartitionMemoryStats*) override;
  void PartitionsDumpBucketStats(
      const char* partition_name,
      const base::PartitionBucketMemoryStats*) override;

  size_t total_mmapped_bytes() const { return total_mmapped_bytes_; }
  size_t total_resident_bytes() const { return total_resident_bytes_; }
  size_t total_active_bytes() const { return total_active_bytes_; }
  uint64_t syscall_count() const { return syscall_count_; }

 private:
  const char* root_name_;
  raw_ptr<base::trace_event::ProcessMemoryDump> memory_dump_;
  uint64_t uid_ = 0;
  size_t total_mmapped_bytes_ = 0;
  size_t total_resident_bytes_ = 0;
  size_t total_active_bytes_ = 0;
  uint64_t syscall_count_ = 0;
  bool detailed_;
};

class MemoryAllocatorDump;

BASE_EXPORT void ReportPartitionAllocThreadCacheStats(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* dump,
    const ThreadCacheStats& stats,
    const std::string& metrics_suffix,
    bool detailed);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_H_
