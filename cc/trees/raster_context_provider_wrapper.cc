// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/raster_context_provider_wrapper.h"

#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/common/gpu/raster_context_provider.h"

namespace cc {

namespace {

bool IsGpuRasterizationEnabled(
    const scoped_refptr<viz::RasterContextProvider>& context) {
  DCHECK(context);
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context.get());

  const gpu::Capabilities& caps = context->ContextCapabilities();
  return caps.gpu_rasterization;
}

bool IsOopRasterSupported(
    const scoped_refptr<viz::RasterContextProvider>& context) {
  DCHECK(context);
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context.get());

  const gpu::Capabilities& caps = context->ContextCapabilities();
  return caps.supports_oop_raster;
}

size_t GetMaxTextureSize(
    const scoped_refptr<viz::RasterContextProvider>& context) {
  DCHECK(context);
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context.get());

  const gpu::Capabilities& caps = context->ContextCapabilities();
  return caps.max_texture_size;
}

}  // namespace

RasterContextProviderWrapper::RasterContextProviderWrapper(
    scoped_refptr<viz::RasterContextProvider> context,
    RasterDarkModeFilter* dark_mode_filter,
    size_t max_working_set_bytes)
    : context_(context),
      context_supports_locking_(!!context_->GetLock()),
      dark_mode_filter_(dark_mode_filter),
      gpu_rasterization_enabled_(IsGpuRasterizationEnabled(context_)),
      supports_oop_raster_(IsOopRasterSupported(context_)),
      max_working_set_bytes_(max_working_set_bytes),
      max_texture_size_(GetMaxTextureSize(context_)) {
  CheckValidThreadOrLockSupported();
}

RasterContextProviderWrapper::~RasterContextProviderWrapper() {
  CheckValidThreadOrLockSupported();
}

void RasterContextProviderWrapper::CheckValidThreadOrLockSupported() const {
#if DCHECK_IS_ON()
  // If the context supports lock, no need to check for a thread.
  if (context_supports_locking_)
    return;
  DCHECK_CALLED_ON_VALID_THREAD(bound_context_thread_checker_);
#endif
}

const scoped_refptr<viz::RasterContextProvider>&
RasterContextProviderWrapper::GetContext() const {
  return context_;
}

GpuImageDecodeCache& RasterContextProviderWrapper::GetGpuImageDecodeCache(
    SkColorType color_type) {
  DCHECK(gpu_rasterization_enabled_ && supports_oop_raster_);

  base::AutoLock scoped_lock(lock_);

  auto cache_iterator = gpu_image_decode_cache_map_.find(color_type);
  if (cache_iterator != gpu_image_decode_cache_map_.end())
    return *cache_iterator->second.get();

  auto insertion_result = gpu_image_decode_cache_map_.emplace(
      color_type,
      std::make_unique<GpuImageDecodeCache>(
          GetContext().get(), /*use_transfer_cache=*/true, color_type,
          max_working_set_bytes_, max_texture_size_, dark_mode_filter_));
  DCHECK(insertion_result.second);
  cache_iterator = insertion_result.first;
  return *cache_iterator->second.get();
}

}  // namespace cc
