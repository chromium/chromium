// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MEMORY_HISTORY_H_
#define CC_RESOURCES_MEMORY_HISTORY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/ring_buffer.h"
#include "base/time/time.h"

namespace cc {

// Maintains a history of memory for each frame.
class MemoryHistory {
 public:
  static std::unique_ptr<MemoryHistory> Create();

  MemoryHistory(const MemoryHistory&) = delete;
  MemoryHistory& operator=(const MemoryHistory&) = delete;

  size_t HistorySize() const { return ring_buffer_.BufferSize(); }

  struct Entry {
    Entry()
        : total_budget_in_bytes(0),
          total_bytes_used(0),
          had_enough_memory(false) {}

    size_t total_budget_in_bytes;
    int64_t total_bytes_used;
    bool had_enough_memory;
  };

  void SaveEntry(const Entry& entry);

  typedef base::RingBuffer<Entry, 80> RingBufferType;
  RingBufferType::Iterator Begin() const { return ring_buffer_.Begin(); }
  RingBufferType::Iterator End() const { return ring_buffer_.End(); }

 private:
  MemoryHistory();

  RingBufferType ring_buffer_;
};

}  // namespace cc

#endif  // CC_RESOURCES_MEMORY_HISTORY_H_
