// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_SCOPED_GRCONTEXT_ACCESS_H_
#define CC_RASTER_SCOPED_GRCONTEXT_ACCESS_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"

// The following class is needed to correctly reset GL state when using a
// GrContext on a RasterInterface enabled context.
class ScopedGrContextAccess {
 public:
  explicit ScopedGrContextAccess(viz::RasterContextProvider* context_provider)
      : context_provider_(context_provider) {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->BeginGpuRaster();
  }
  ~ScopedGrContextAccess() {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->EndGpuRaster();
  }

 private:
  raw_ptr<viz::RasterContextProvider> context_provider_;
};

#endif  // CC_RASTER_SCOPED_GRCONTEXT_ACCESS_H_
