// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_

#include <vector>

#include "base/atomic_sequence_num.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

// Client/ServiceRawMemoryTransferCacheEntry implement a transfer cache entry
// backed by raw memory, with no conversion during serialization or
// deserialization.
class CC_PAINT_EXPORT ClientRawMemoryTransferCacheEntry final
    : public ClientTransferCacheEntryBase<TransferCacheEntryType::kRawMemory> {
 public:
  explicit ClientRawMemoryTransferCacheEntry(std::vector<uint8_t> data);
  ~ClientRawMemoryTransferCacheEntry() final;
  uint32_t Id() const final;
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

 private:
  uint32_t id_;
  std::vector<uint8_t> data_;
  static base::AtomicSequenceNumber s_next_id_;
};

class CC_PAINT_EXPORT ServiceRawMemoryTransferCacheEntry final
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kRawMemory> {
 public:
  ServiceRawMemoryTransferCacheEntry();
  ~ServiceRawMemoryTransferCacheEntry() final;
  size_t CachedSize() const final;
  bool Deserialize(GrDirectContext* context,
                   skgpu::graphite::Recorder* graphite_recorder,
                   base::span<const uint8_t> data) final;
  const std::vector<uint8_t>& data() { return data_; }

 private:
  std::vector<uint8_t> data_;
};

}  // namespace cc

#endif  // CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_
