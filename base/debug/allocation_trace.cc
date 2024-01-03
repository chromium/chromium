// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/allocation_trace.h"

#include <array>
#include <atomic>

#include "base/check_op.h"

namespace base::debug::tracer {

bool OperationRecord::IsRecording() const {
  if (is_recording_.test_and_set()) {
    return true;
  }

  is_recording_.clear();
  return false;
}

OperationType OperationRecord::GetOperationType() const {
  return operation_type_;
}

const void* OperationRecord::GetAddress() const {
  return address_;
}

size_t OperationRecord::GetSize() const {
  return size_;
}

const StackTraceContainer& OperationRecord::GetStackTrace() const {
  return stack_trace_;
}

#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
AllocationTraceRecorderStatistics::AllocationTraceRecorderStatistics(
    size_t total_number_of_allocations,
    size_t total_number_of_collisions)
    : total_number_of_allocations(total_number_of_allocations),
      total_number_of_collisions(total_number_of_collisions) {}
#else
AllocationTraceRecorderStatistics::AllocationTraceRecorderStatistics(
    size_t total_number_of_allocations)
    : total_number_of_allocations(total_number_of_allocations) {}
#endif

void AllocationTraceRecorder::OnAllocation(const void* allocated_address,
                                           size_t allocated_size) {
  // Record the allocation into the next available slot, allowing for failure
  // due to the slot already being in-use by another
  // OperationRecord::Initialize*() call from another thread.
  for (auto index = GetNextIndex();
       !alloc_trace_buffer_[index].InitializeAllocation(allocated_address,
                                                        allocated_size);
       index = GetNextIndex()) {
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
    total_number_of_collisions_.fetch_add(1, std::memory_order_relaxed);
#endif
  }
}

void AllocationTraceRecorder::OnFree(const void* freed_address) {
  // Record the free into the next available slot, allowing for failure due to
  // the slot already being in-use by another OperationRecord::Initialize*()
  // call from another thread.
  for (auto index = GetNextIndex();
       !alloc_trace_buffer_[index].InitializeFree(freed_address);
       index = GetNextIndex()) {
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
    total_number_of_collisions_.fetch_add(1, std::memory_order_relaxed);
#endif
  }
}

size_t AllocationTraceRecorder::size() const {
  return std::min(kMaximumNumberOfMemoryOperationTraces,
                  total_number_of_records_.load(std::memory_order_relaxed));
}

const OperationRecord& AllocationTraceRecorder::operator[](size_t idx) const {
  DCHECK_LT(idx, size());

  const size_t array_index =
      size() < GetMaximumNumberOfTraces()
          ? idx
          : WrapIdxIfNeeded(
                total_number_of_records_.load(std::memory_order_relaxed) + idx);

  DCHECK_LT(array_index, alloc_trace_buffer_.size());

  return alloc_trace_buffer_[array_index];
}

AllocationTraceRecorderStatistics
AllocationTraceRecorder::GetRecorderStatistics() const {
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
  return {total_number_of_records_.load(std::memory_order_relaxed),
          total_number_of_collisions_.load(std::memory_order_relaxed)};
#else
  return {total_number_of_records_.load(std::memory_order_relaxed)};
#endif
}

}  // namespace base::debug::tracer