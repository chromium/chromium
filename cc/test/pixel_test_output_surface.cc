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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PixelTestOutputSurface::SwapBuffersCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PixelTestOutputSurface::SwapBuffersCallback() {
  base::TimeTicks now = base::TimeTicks::Now();
  gfx::SwapTimings timings = {now, now};
  client_->DidReceiveSwapBuffersAck(timings,
                                    /*release_fence=*/gfx::GpuFenceHandle());
  client_->DidReceivePresentationFeedback(
      gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(), 0));
}

bool PixelTestOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

void PixelTestOutputSurface::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {}

gfx::OverlayTransform PixelTestOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace cc
