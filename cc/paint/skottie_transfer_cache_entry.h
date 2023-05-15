// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_SKOTTIE_TRANSFER_CACHE_ENTRY_H_

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

class SkottieWrapper;

// Client/ServiceSkottieTransferCacheEntry implements a transfer cache entry
// for transferring skottie data.
class CC_PAINT_EXPORT ClientSkottieTransferCacheEntry final
    : public ClientTransferCacheEntryBase<TransferCacheEntryType::kSkottie> {
 public:
  explicit ClientSkottieTransferCacheEntry(
      scoped_refptr<SkottieWrapper> skottie);
  ~ClientSkottieTransferCacheEntry() final;

  uint32_t Id() const final;

  // ClientTransferCacheEntry implementation:
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

 private:
  scoped_refptr<SkottieWrapper> skottie_;
};

class CC_PAINT_EXPORT ServiceSkottieTransferCacheEntry final
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kSkottie> {
 public:
  ServiceSkottieTransferCacheEntry();
  ~ServiceSkottieTransferCacheEntry() final;

  // ServiceTransferCacheEntry implementation:
  size_t CachedSize() const final;
  bool Deserialize(GrDirectContext* context,
                   skgpu::graphite::Recorder* graphite_recorder,
                   base::span<const uint8_t> data) final;

  const scoped_refptr<SkottieWrapper>& skottie() const { return skottie_; }

 private:
  scoped_refptr<SkottieWrapper> skottie_;
  uint32_t cached_size_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_TRANSFER_CACHE_ENTRY_H_
