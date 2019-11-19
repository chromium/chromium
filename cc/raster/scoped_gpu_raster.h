// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_SCOPED_GPU_RASTER_H_
#define CC_RASTER_SCOPED_GPU_RASTER_H_

#include <memory>

#include "base/logging.h"
#include "cc/cc_export.h"

namespace viz {
class ContextProvider;
}  // namespace viz

namespace cc {

// The following class is needed to modify GL resources using GPU
// raster. The user must ensure that they only use GPU raster on
// GL resources while an instance of this class is alive.
class CC_EXPORT ScopedGpuRaster {
 public:
  explicit ScopedGpuRaster(viz::ContextProvider* context_provider);
  ScopedGpuRaster(const ScopedGpuRaster&) = delete;
  ~ScopedGpuRaster();

  ScopedGpuRaster& operator=(const ScopedGpuRaster&) = delete;

 private:
  void BeginGpuRaster();
  void EndGpuRaster();

  viz::ContextProvider* context_provider_;
};

}  // namespace cc

#endif  // CC_RASTER_SCOPED_GPU_RASTER_H_
