// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/instance_tracer.h"

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_root.h"

namespace base::internal {

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_INSTANCE_TRACER)

static_assert(BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT),
              "Instance tracing requires BackupRefPtr support.");

namespace {

struct Info {
  explicit Info(uintptr_t slot_count) : slot_count(slot_count) {
    partition_alloc::internal::base::debug::CollectStackTrace(
        stack_trace.data(), stack_trace.size());
  }

  uintptr_t slot_count;
  std::array<const void*, 32> stack_trace = {};
};

auto& GetStorage() {
  static partition_alloc::internal::base::NoDestructor<std::map<uint64_t, Info>>
      storage;
  return *storage;
}

auto& GetStorageMutex() {
  static partition_alloc::internal::base::NoDestructor<std::mutex>
      storage_mutex;
  return *storage_mutex;
}

}  // namespace

std::atomic<uint64_t> InstanceTracer::counter_ = 0;

void InstanceTracer::TraceImpl(uint64_t owner_id, uintptr_t address) {
  PA_CHECK(owner_id);
  const std::pair<uintptr_t, size_t> slot_and_size =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(address);
  const uintptr_t slot_count = reinterpret_cast<uintptr_t>(
      partition_alloc::PartitionRoot::RefCountPointerFromSlotStartAndSize(
          slot_and_size.first, slot_and_size.second));

  const std::lock_guard guard(GetStorageMutex());
  GetStorage().insert({owner_id, Info(slot_count)});
}

void InstanceTracer::UntraceImpl(uint64_t owner_id) {
  PA_CHECK(owner_id);
  const std::lock_guard guard(GetStorageMutex());
  GetStorage().erase(owner_id);
}

std::vector<std::array<const void*, 32>>
InstanceTracer::GetStackTracesForDanglingRefs(uintptr_t allocation) {
  std::vector<std::array<const void*, 32>> result;
  const std::lock_guard guard(GetStorageMutex());
  for (const auto& [id, info] : GetStorage()) {
    if (info.slot_count == allocation) {
      result.push_back(info.stack_trace);
    }
  }
  return result;
}

std::vector<std::array<const void*, 32>>
InstanceTracer::GetStackTracesForAddressForTest(const void* address) {
  const std::pair<uintptr_t, size_t> slot_and_size =
      partition_alloc::PartitionAllocGetSlotStartAndSizeInBRPPool(
          reinterpret_cast<uintptr_t>(address));
  const uintptr_t slot_count = reinterpret_cast<uintptr_t>(
      partition_alloc::PartitionRoot::RefCountPointerFromSlotStartAndSize(
          slot_and_size.first, slot_and_size.second));
  return GetStackTracesForDanglingRefs(slot_count);
}

#endif

}  // namespace base::internal
