// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RASTER_CONTEXT_PROVIDER_WRAPPER_H_
#define CC_TREES_RASTER_CONTEXT_PROVIDER_WRAPPER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "cc/trees/raster_capabilities.h"
#include "third_party/skia/include/core/SkColorType.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace cc {

class RasterDarkModeFilter;
class GpuImageDecodeCache;

// A wrapper of a worker context that is responsible for creation and
// maintenance of GpuImageDecodeCache instances. These caches are created only
// when GPU rasterization is enabled and are meant to be shared across clients.
// If a parameter passed along with a request for a decode cache match to
// already existing cache, an existing cache is returned. This helps to share
// already decoded images between clients (eg 1+n browser windows).
// This class must be created, used and destroyed on the thread the underlying
// context is bound to. Or on a thread where the underlying context can be used
// if it supports locking.
class CC_EXPORT RasterContextProviderWrapper
    : public base::RefCountedThreadSafe<RasterContextProviderWrapper> {
 public:
  RasterContextProviderWrapper(
      scoped_refptr<viz::RasterContextProvider> context,
      RasterDarkModeFilter* dark_mode_filter,
      size_t max_working_set_bytes);
  RasterContextProviderWrapper(const RasterContextProviderWrapper&) = delete;
  RasterContextProviderWrapper& operator=(const RasterContextProviderWrapper&) =
      delete;

  const scoped_refptr<viz::RasterContextProvider>& GetContext() const;

  // This should only be called from a thread which can use the underlying
  // context. It's responsibility of the caller to ensure the context is bound
  // to the current thread.
  GpuImageDecodeCache& GetGpuImageDecodeCache(
      SkColorType color_type,
      const RasterCapabilities& raster_caps);

 private:
  friend class base::RefCountedThreadSafe<RasterContextProviderWrapper>;

  ~RasterContextProviderWrapper();

  void CheckValidThreadOrLockSupported() const;

  void OnAllClientsVisibilityChanged(bool visible);

  // The worker context that this wrapper holds.
  const scoped_refptr<viz::RasterContextProvider> context_;

  // Identifies if the |context| supports locking. See more details in the
  // comment to this class.
  const bool context_supports_locking_;

  // A filter that will be used by GpuImageDecodeCache instances.
  const raw_ptr<RasterDarkModeFilter> dark_mode_filter_;

  // The following are passed to GpuImageDecodeCache instances:
  // The budget size in bytes of decoded image working set.
  const size_t max_working_set_bytes_;

  // Protects access to |gpu_image_decode_cache_map_|.
  base::Lock lock_;

  // Maintains the map of shared decode caches to be shared across clients of
  // the same raster context. Guarded by |lock_|.
  base::flat_map<SkColorType, std::unique_ptr<GpuImageDecodeCache>>
      gpu_image_decode_cache_map_ GUARDED_BY(lock_);

  THREAD_CHECKER(bound_context_thread_checker_);
};

}  // namespace cc

#endif  // CC_TREES_RASTER_CONTEXT_PROVIDER_WRAPPER_H_
