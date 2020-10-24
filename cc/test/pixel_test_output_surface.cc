// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gfx/transform.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<viz::ContextProvider> context_provider,
    gfx::SurfaceOrigin origin)
    : OutputSurface(std::move(context_provider)) {
  capabilities_.output_surface_origin = origin;
  capabilities_.supports_stencil = true;
}

PixelTestOutputSurface::PixelTestOutputSurface(
    std::unique_ptr<viz::SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)) {
  capabilities_.supports_stencil = true;
}

PixelTestOutputSurface::~PixelTestOutputSurface() = default;

void PixelTestOutputSurface::BindToClient(viz::OutputSurfaceClient* client) {
  client_ = client;
}

void PixelTestOutputSurface::EnsureBackbuffer() {}

void PixelTestOutputSurface::DiscardBackbuffer() {}

void PixelTestOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PixelTestOutputSurface::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     gfx::BufferFormat format,
                                     bool use_stencil) {
  // External stencil test cannot be tested at the same time as |use_stencil|.
  DCHECK(!use_stencil || !external_stencil_test_);
  if (context_provider()) {
    const bool has_alpha = gfx::AlphaBitsForBufferFormat(format);
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        color_space.AsGLColorSpace(), has_alpha);
  } else {
    software_device()->Resize(size, device_scale_factor);
  }
}

bool PixelTestOutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_;
}

void PixelTestOutputSurface::ApplyExternalStencil() {}

void PixelTestOutputSurface::SwapBuffers(viz::OutputSurfaceFrame frame) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PixelTestOutputSurface::SwapBuffersCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PixelTestOutputSurface::SwapBuffersCallback() {
  base::TimeTicks now = base::TimeTicks::Now();
  gfx::SwapTimings timings = {now, now};
  client_->DidReceiveSwapBuffersAck(timings);
  client_->DidReceivePresentationFeedback(
      gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(), 0));
}

bool PixelTestOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned PixelTestOutputSurface::GetOverlayTextureId() const {
  return 0;
}

uint32_t PixelTestOutputSurface::GetFramebufferCopyTextureFormat() {
  // This format will work if the |context_provider| has an RGB or RGBA
  // framebuffer. For now assume tests do not want/care about alpha in
  // the root render pass.
  return GL_RGB;
}

unsigned PixelTestOutputSurface::UpdateGpuFence() {
  return 0;
}

void PixelTestOutputSurface::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {}

gfx::OverlayTransform PixelTestOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace cc
