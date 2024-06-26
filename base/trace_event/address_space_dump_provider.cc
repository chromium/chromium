// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/address_space_dump_provider.h"

#include "base/no_destructor.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"

namespace base::trace_event {

namespace {

using ::partition_alloc::internal::kSuperPageSize;

// Implements the rendezvous interface that shuttles figures out of the
// `AddressSpaceStatsDumper`.
class AddressSpaceStatsDumperImpl final
    : public partition_alloc::AddressSpaceStatsDumper {
 public:
  explicit AddressSpaceStatsDumperImpl(ProcessMemoryDump* memory_dump)
      : memory_dump_(memory_dump) {}
  ~AddressSpaceStatsDumperImpl() final = default;

  void DumpStats(
      const partition_alloc::AddressSpaceStats* address_space_stats) override {
    MemoryAllocatorDump* dump =
        memory_dump_->CreateAllocatorDump("partition_alloc/address_space");

    // Regular pool usage is applicable everywhere.
    dump->AddScalar(
        "regular_pool_usage", MemoryAllocatorDump::kUnitsBytes,
        address_space_stats->regular_pool_stats.usage * kSuperPageSize);

    // BRP pool usage is applicable with the appropriate buildflag.
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    dump->AddScalar("brp_pool_usage", MemoryAllocatorDump::kUnitsBytes,
                    address_space_stats->brp_pool_stats.usage * kSuperPageSize);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

    // The configurable pool is only available on 64-bit platforms.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    dump->AddScalar(
        "configurable_pool_usage", MemoryAllocatorDump::kUnitsBytes,
        address_space_stats->configurable_pool_stats.usage * kSuperPageSize);
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

    // Thread isolated pool usage is applicable with the appropriate buildflag.
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    dump->AddScalar(
        "thread_isolated_pool_usage", MemoryAllocatorDump::kUnitsBytes,
        address_space_stats->thread_isolated_pool_stats.usage * kSuperPageSize);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

    // Additionally, largest possible reservation is also available on
    // 64-bit platforms.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    dump->AddScalar(
        "regular_pool_largest_reservation", MemoryAllocatorDump::kUnitsBytes,
        address_space_stats->regular_pool_stats.largest_available_reservation *
            kSuperPageSize);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    dump->AddScalar(
        "brp_pool_largest_reservation", MemoryAllocatorDump::kUnitsBytes,
        address_space_stats->brp_pool_stats.largest_available_reservation *
            kSuperPageSize);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    dump->AddScalar("configurable_pool_largest_reservation",
                    MemoryAllocatorDump::kUnitsBytes,
                    address_space_stats->configurable_pool_stats
                            .largest_available_reservation *
                        kSuperPageSize);
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    dump->AddScalar("thread_isolated_pool_largest_reservation",
                    MemoryAllocatorDump::kUnitsBytes,
                    address_space_stats->thread_isolated_pool_stats
                            .largest_available_reservation *
                        kSuperPageSize);
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    dump->AddScalar("blocklist_size", MemoryAllocatorDump::kUnitsObjects,
                    address_space_stats->blocklist_size);
    dump->AddScalar("blocklist_hit_count", MemoryAllocatorDump::kUnitsObjects,
                    address_space_stats->blocklist_hit_count);
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS) &&
        // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    return;
  }

 private:
  raw_ptr<base::trace_event::ProcessMemoryDump> memory_dump_;
};

}  // namespace

AddressSpaceDumpProvider::AddressSpaceDumpProvider() = default;
AddressSpaceDumpProvider::~AddressSpaceDumpProvider() = default;

// static
AddressSpaceDumpProvider* AddressSpaceDumpProvider::GetInstance() {
  static base::NoDestructor<AddressSpaceDumpProvider> instance;
  return instance.get();
}

// MemoryDumpProvider implementation.
bool AddressSpaceDumpProvider::OnMemoryDump(const MemoryDumpArgs& args,
                                            ProcessMemoryDump* pmd) {
  AddressSpaceStatsDumperImpl stats_dumper(pmd);
  partition_alloc::internal::AddressPoolManager::GetInstance().DumpStats(
      &stats_dumper);
  return true;
}

}  // namespace base::trace_event
