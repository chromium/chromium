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

void AllocationTraceRecorder::OnAllocation(
    const void* allocated_address,
    size_t allocated_size,
    base::allocator::dispatcher::AllocationSubsystem subsystem,
    const char* type) {
  for (auto idx = GetNextIndex();
       !alloc_trace_buffer_[idx].InitializeAllocation(allocated_address,
                                                      allocated_size);
       idx = GetNextIndex()) {
  }
}

void AllocationTraceRecorder::OnFree(const void* freed_address) {
  for (auto idx = GetNextIndex();
       !alloc_trace_buffer_[idx].InitializeFree(freed_address);
       idx = GetNextIndex()) {
  }
}

bool AllocationTraceRecorder::IsValid() const {
  return kMemoryGuard == prologue_ && kMemoryGuard == epilogue_;
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

}  // namespace base::debug::tracer