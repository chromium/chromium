// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/scoped_gpu_raster.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

using gpu::gles2::GLES2Interface;

namespace cc {

ScopedGpuRaster::ScopedGpuRaster(viz::ContextProvider* context_provider)
    : context_provider_(context_provider) {
  BeginGpuRaster();
}

ScopedGpuRaster::~ScopedGpuRaster() {
  EndGpuRaster();
}

void ScopedGpuRaster::BeginGpuRaster() {
  GLES2Interface* gl = context_provider_->ContextGL();

  // TODO(alokp): Use a trace macro to push/pop markers.
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl->TraceBeginCHROMIUM("ScopedGpuRaster", "GpuRasterization");

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/832810): The following reset should not be necessary.
  GrDirectContext* gr_context = context_provider_->GrContext();
  gr_context->resetContext();
#endif
}

void ScopedGpuRaster::EndGpuRaster() {
  GLES2Interface* gl = context_provider_->ContextGL();

  // Restore default GL unpack alignment.  TextureUploader expects this.
  gl->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

  // TODO(alokp): Use a trace macro to push/pop markers.
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl->TraceEndCHROMIUM();
}

}  // namespace cc
