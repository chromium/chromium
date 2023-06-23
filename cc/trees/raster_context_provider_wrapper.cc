// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/raster_context_provider_wrapper.h"

#include "base/functional/bind.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/common/gpu/raster_context_provider.h"

namespace cc {

RasterContextProviderWrapper::RasterContextProviderWrapper(
    scoped_refptr<viz::RasterContextProvider> context,
    RasterDarkModeFilter* dark_mode_filter,
    size_t max_working_set_bytes)
    : context_(context),
      context_supports_locking_(!!context_->GetLock()),
      dark_mode_filter_(dark_mode_filter),
      max_working_set_bytes_(max_working_set_bytes) {
  CheckValidThreadOrLockSupported();

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context.get());
  // This callback can use a raw ptr for the cb as the wrapper outlive the cache
  // controller.
  context_->CacheController()->SetNotifyAllClientsVisibilityChangedCb(
      base::BindRepeating(
          &RasterContextProviderWrapper::OnAllClientsVisibilityChanged,
          base::Unretained(this)));
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

void RasterContextProviderWrapper::OnAllClientsVisibilityChanged(bool visible) {
  // Once all the clients are invisible, we should aggressively free resources
  // from the image decode caches. This what ContextCacheController also does -
  // it notifies the context support it must aggressively free resources if
  // all clients that share the same context became invisible.
  const bool should_aggressively_free_resources = !visible;

  // The caller of
  // ContextCacheController::ClientBecomeVisible/ClientBecomeNotVisible must
  // acquire lock. Thus, we are called with lock acquired. Unfortunately, we
  // have to either make ImageDecodeCache require acquiring lock or provide it
  // with information that lock has been acquired. Otherwise, a deadlock
  // happens.
  //
  // If lock is not supported, lock has not been acquired.
  const bool context_lock_acquired = context_supports_locking_;
  if (context_supports_locking_)
    context_->GetLock()->AssertAcquired();

  base::AutoLock scoped_lock(lock_);
  for (const auto& item : gpu_image_decode_cache_map_) {
    item.second->SetShouldAggressivelyFreeResources(
        should_aggressively_free_resources, context_lock_acquired);
  }
}

const scoped_refptr<viz::RasterContextProvider>&
RasterContextProviderWrapper::GetContext() const {
  return context_;
}

GpuImageDecodeCache& RasterContextProviderWrapper::GetGpuImageDecodeCache(
    SkColorType color_type,
    const RasterCapabilities& raster_caps) {
  DCHECK(raster_caps.use_gpu_rasterization);
  base::AutoLock scoped_lock(lock_);

  auto cache_iterator = gpu_image_decode_cache_map_.find(color_type);
  if (cache_iterator != gpu_image_decode_cache_map_.end())
    return *cache_iterator->second.get();

  auto insertion_result = gpu_image_decode_cache_map_.emplace(
      color_type, std::make_unique<GpuImageDecodeCache>(
                      GetContext().get(), /*use_transfer_cache=*/true,
                      color_type, max_working_set_bytes_,
                      raster_caps.max_texture_size, dark_mode_filter_));
  DCHECK(insertion_result.second);
  cache_iterator = insertion_result.first;
  return *cache_iterator->second.get();
}

}  // namespace cc
