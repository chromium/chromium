// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SHADER_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_SHADER_TRANSFER_CACHE_ENTRY_H_

#include "base/containers/span.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

// There is only a service transfer cache entry here.  The reason shaders
// are cached at all are to reuse internal Skia caches for SkPictureShaders.
// However, the major reason not to transfer from the client is that it
// avoids the design change to make it possible for transfer cache entries
// to depend on transfer cache entries.  This adds a number of wrinkles
// (during serialization, deserialization, scheduling).  The assumption
// is that most picture shaders are small (e.g. a few ops to draw a tiled
// image) and that the design complication for this edge case isn't worth
// it.

class CC_PAINT_EXPORT ServiceShaderTransferCacheEntry final
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kShader> {
 public:
  explicit ServiceShaderTransferCacheEntry(sk_sp<PaintShader> shader,
                                           size_t size);
  ~ServiceShaderTransferCacheEntry() final;
  size_t CachedSize() const final;
  bool Deserialize(GrDirectContext* context,
                   skgpu::graphite::Recorder* graphite_recorder,
                   base::span<const uint8_t> data) final;

  sk_sp<PaintShader> shader() const { return shader_; }

 private:
  sk_sp<PaintShader> shader_;
  size_t size_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_SHADER_TRANSFER_CACHE_ENTRY_H_
