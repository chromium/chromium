// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    std::unique_ptr<viz::SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)) {}

PixelTestOutputSurface::~PixelTestOutputSurface() = default;

void PixelTestOutputSurface::BindToClient(viz::OutputSurfaceClient* client) {
  client_ = client;
}

void PixelTestOutputSurface::EnsureBackbuffer() {}

void PixelTestOutputSurface::DiscardBackbuffer() {}

void PixelTestOutputSurface::Reshape(const ReshapeParams& params) {
  software_device()->Resize(params.size, params.device_scale_factor);
}

void PixelTestOutputSurface::SwapBuffers(viz::OutputSurfaceFrame frame) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PixelTestOutputSurface::SwapBuffersCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PixelTestOutputSurface::SwapBuffersCallback() {
  base::TimeTicks now = base::TimeTicks::Now();
  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = {now, now};
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  client_->DidReceiveSwapBuffersAck(params,
                                    /*release_fence=*/gfx::GpuFenceHandle());
  client_->DidReceivePresentationFeedback(
      gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(), 0));
}

void PixelTestOutputSurface::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {}

gfx::OverlayTransform PixelTestOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

void PixelTestOutputSurface::ReadbackForTesting(
    base::OnceCallback<void(std::unique_ptr<viz::CopyOutputResult>)> callback) {
  SkBitmap bitmap = software_device()->ReadbackForTesting();
  std::move(callback).Run(std::make_unique<viz::CopyOutputSkBitmapResult>(
      gfx::Rect(gfx::SkISizeToSize(bitmap.dimensions())), std::move(bitmap)));
}

}  // namespace cc
