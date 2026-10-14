// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_TIMING_BUFFER_H_
#define CC_METRICS_SCROLL_TIMING_BUFFER_H_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/shared_memory_safety_checker.h"
#include "base/memory/structured_shared_memory.h"
#include "cc/cc_export.h"
#include "cc/metrics/scroll_timing_info.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}

namespace cc {

// Producer for completed scroll timing records.
// Push() may run concurrently with ScrollTimingBufferConsumer::ReadRecords()
// in the renderer, but neither operation may run concurrently with itself.
class CC_EXPORT ScrollTimingBuffer {
 public:
  // Matches Blink's kDefaultScrollTimingBufferSize.
  static constexpr uint32_t kCapacity = 150;

  struct CC_EXPORT Slot {
    // Seqlock: 2 * (index + 1) once the slot holds the record at `index`, odd
    // while Push() overwrites it.
    base::subtle::SharedAtomic<uint64_t> sequence{0};
    // Accessed through std::atomic_ref because reads can race with Push().
    ScrollTimingInfo record{};
  };

  struct CC_EXPORT SharedState {
    base::subtle::SharedAtomic<uint64_t> write_count{0};
    // Records are indexed from 0 in push order. The record at `index` is in
    // slots[index % kCapacity].
    std::array<Slot, kCapacity> slots{};
  };

  // Returns null on allocation/mapping failure.
  static std::unique_ptr<ScrollTimingBuffer> Create();

  ScrollTimingBuffer(const ScrollTimingBuffer&) = delete;
  ScrollTimingBuffer& operator=(const ScrollTimingBuffer&) = delete;
  ~ScrollTimingBuffer();

  // Overwrites the oldest record when full.
  void Push(const ScrollTimingInfo& record);

  base::ReadOnlySharedMemoryRegion DuplicateReadOnlyRegion() const;

 private:
  explicit ScrollTimingBuffer(
      base::StructuredSharedMemory<SharedState> storage);

  base::StructuredSharedMemory<SharedState> storage_;
};

// Reads a ScrollTimingBuffer without writing to it, tracking its own read
// position.
class CC_EXPORT ScrollTimingBufferConsumer {
 public:
  struct CC_EXPORT Snapshot {
    std::vector<ScrollTimingInfo> records;
    // Records overwritten before they could be read.
    uint64_t overwritten_count = 0;
  };

  // Returns null on mapping failure.
  static std::unique_ptr<ScrollTimingBufferConsumer> Create(
      base::ReadOnlySharedMemoryRegion region);

  ScrollTimingBufferConsumer(const ScrollTimingBufferConsumer&) = delete;
  ScrollTimingBufferConsumer& operator=(const ScrollTimingBufferConsumer&) =
      delete;
  ~ScrollTimingBufferConsumer();

  // Returns records pushed since the previous call, oldest-first. The first
  // call returns all retained records.
  Snapshot ReadRecords();

 private:
  explicit ScrollTimingBufferConsumer(
      base::StructuredSharedMemory<
          ScrollTimingBuffer::SharedState>::ReadOnlyMapping storage);

  base::StructuredSharedMemory<ScrollTimingBuffer::SharedState>::ReadOnlyMapping
      storage_;
  // Index of the next record to read.
  uint64_t next_index_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_TIMING_BUFFER_H_
