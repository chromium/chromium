// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TEXTBLOB_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_TEXTBLOB_TRANSFER_CACHE_ENTRY_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/transfer_cache_entry.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {

class CC_PAINT_EXPORT ServiceTextBlobTransferCacheEntry
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kTextBlob> {
 public:
  ServiceTextBlobTransferCacheEntry(sk_sp<SkTextBlob> blob, size_t size);
  ~ServiceTextBlobTransferCacheEntry() final;
  size_t CachedSize() const final;
  bool Deserialize(GrContext* context, base::span<const uint8_t> data) final;

  const sk_sp<SkTextBlob>& blob() const { return blob_; }

 private:
  sk_sp<SkTextBlob> blob_;
  const size_t size_;
};

}  // namespace cc

#endif  // CC_PAINT_TEXTBLOB_TRANSFER_CACHE_ENTRY_H_
