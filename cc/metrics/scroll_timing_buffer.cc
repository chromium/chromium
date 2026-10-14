// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_timing_buffer.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"

namespace cc {

ScrollTimingBuffer::ScrollTimingBuffer(
    base::StructuredSharedMemory<SharedState> storage)
    : storage_(std::move(storage)) {}

ScrollTimingBuffer::~ScrollTimingBuffer() = default;

// static
std::unique_ptr<ScrollTimingBuffer> ScrollTimingBuffer::Create() {
  auto storage = base::StructuredSharedMemory<SharedState>::Create();
  if (!storage) {
    return nullptr;
  }
  return base::WrapUnique(new ScrollTimingBuffer(std::move(*storage)));
}

void ScrollTimingBuffer::Push(const ScrollTimingInfo& record) {
  SharedState& state = storage_.WritableRef();
  const uint64_t index = state.write_count.load(std::memory_order_relaxed);
  Slot& slot = state.slots[index % kCapacity];
  // Seqlock write, as in device::OneWriterSeqLock::WriteBegin()/WriteEnd().
  slot.sequence.store(2 * index + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  std::atomic_ref(slot.record).store(record, std::memory_order_relaxed);
  slot.sequence.store(2 * index + 2, std::memory_order_release);
  state.write_count.store(index + 1, std::memory_order_release);
}

base::ReadOnlySharedMemoryRegion ScrollTimingBuffer::DuplicateReadOnlyRegion()
    const {
  return storage_.DuplicateReadOnlyRegion();
}

ScrollTimingBufferConsumer::ScrollTimingBufferConsumer(
    base::StructuredSharedMemory<
        ScrollTimingBuffer::SharedState>::ReadOnlyMapping storage)
    : storage_(std::move(storage)) {}

ScrollTimingBufferConsumer::~ScrollTimingBufferConsumer() = default;

// static
std::unique_ptr<ScrollTimingBufferConsumer> ScrollTimingBufferConsumer::Create(
    base::ReadOnlySharedMemoryRegion region) {
  auto storage = base::StructuredSharedMemory<
      ScrollTimingBuffer::SharedState>::MapReadOnlyRegion(std::move(region));
  if (!storage) {
    return nullptr;
  }
  return base::WrapUnique(new ScrollTimingBufferConsumer(std::move(*storage)));
}

ScrollTimingBufferConsumer::Snapshot ScrollTimingBufferConsumer::ReadRecords() {
  constexpr uint64_t kCapacity = ScrollTimingBuffer::kCapacity;
  const auto& state = storage_.ReadOnlyRef();
  // Pairs with Push()'s write_count release. Read once to bound the copy while
  // the producer keeps writing.
  const uint64_t end = state.write_count.load(std::memory_order_acquire);
  if (end < next_index_) {
    DLOG(ERROR) << "Scroll Timing buffer write count moved backwards";
    return {};
  }
  // Skips records already returned or already overwritten.
  const uint64_t begin = std::max(next_index_, end - std::min(end, kCapacity));
  Snapshot snapshot;
  snapshot.overwritten_count = begin - next_index_;
  snapshot.records.reserve(end - begin);
  for (uint64_t index = begin; index < end; ++index) {
    const auto& slot = state.slots[index % kCapacity];
    // Seqlock read, as in device::OneWriterSeqLock::ReadBegin()/ReadRetry().
    // Any other sequence means Push() has started overwriting the record.
    const uint64_t sequence = 2 * index + 2;
    const uint64_t sequence_before =
        slot.sequence.load(std::memory_order_acquire);
    // TODO(https://github.com/llvm/llvm-project/issues/118378): Remove
    // const_cast.
    const ScrollTimingInfo record =
        std::atomic_ref(const_cast<ScrollTimingInfo&>(slot.record))
            .load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (sequence_before == sequence &&
        slot.sequence.load(std::memory_order_relaxed) == sequence) {
      if (record.input_type < ui::ScrollInputType::kTouchscreen ||
          record.input_type > ui::ScrollInputType::kMaxValue) {
        DLOG(ERROR) << "Invalid Scroll Timing input type: "
                    << static_cast<int>(record.input_type);
        continue;
      }
      snapshot.records.push_back(record);
    } else {
      ++snapshot.overwritten_count;
    }
  }
  next_index_ = end;
  return snapshot;
}

}  // namespace cc
