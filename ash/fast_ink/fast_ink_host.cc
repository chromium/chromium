// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/fast_ink/fast_ink_host_frame_utils.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/video_types.h"

namespace ash {

// -----------------------------------------------------------------------------
// FastInkHost::ScopedPaint

FastInkHost::ScopedPaint::ScopedPaint(const FastInkHost* host,
                                      const gfx::Rect& damage_rect_in_window)
    : gpu_memory_buffer_(host->gpu_memory_buffer_.get()),
      damage_rect_(fast_ink_internal::BufferRectFromWindowRect(
          host->window_to_buffer_transform_,
          gpu_memory_buffer_->GetSize(),
          damage_rect_in_window)),
      canvas_(damage_rect_.size(), 1.0f, false) {
  canvas_.Translate(-damage_rect_.OffsetFromOrigin());
  canvas_.Transform(host->window_to_buffer_transform_);
}

FastInkHost::ScopedPaint::~ScopedPaint() {
  if (damage_rect_.IsEmpty()) {
    return;
  }

  {
    // TODO(zoraiznaeem): Investigate the precision as we will get non trivial
    // additional time of printing the error log.
    TRACE_EVENT0("ui", "FastInkHost::ScopedPaint::Map");

    if (!gpu_memory_buffer_->Map()) {
      LOG(ERROR) << "Failed to map GPU memory buffer";
      return;
    }
  }

  // Copy result to GPU memory buffer. This is effectively a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "FastInkHost::ScopedPaint::Copy", "damage_rect",
                 damage_rect_.ToString());

    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas_.GetBitmap().readPixels(
        SkImageInfo::MakeN32Premul(damage_rect_.width(), damage_rect_.height()),
        data + damage_rect_.y() * stride + damage_rect_.x() * 4, stride, 0, 0);
  }

  {
    TRACE_EVENT0("ui", "FastInkHost::UpdateBuffer::Unmap");

    // Unmap to flush writes to buffer.
    gpu_memory_buffer_->Unmap();
  }
}

// -----------------------------------------------------------------------------
// FastInkHost:

FastInkHost::FastInkHost() = default;
FastInkHost::~FastInkHost() = default;

void FastInkHost::Init(aura::Window* host_window) {
  InitializeFastInkBuffer(host_window);
  FrameSinkHost::Init(host_window);
}

void FastInkHost::InitForTesting(
    aura::Window* host_window,
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  InitializeFastInkBuffer(host_window);
  FrameSinkHost::InitForTesting(host_window, std::move(layer_tree_frame_sink));
}

void FastInkHost::InitializeFastInkBuffer(aura::Window* host_window) {
  // Take the root transform and apply this during buffer update instead of
  // leaving this up to the compositor. The benefit is that HW requirements
  // for being able to take advantage of overlays and direct scanout are
  // reduced significantly. Frames are submitted to the compositor with the
  // inverse transform to cancel out the transformation that would otherwise
  // be done by the compositor.
  window_to_buffer_transform_ = host_window->GetHost()->GetRootTransform();
  gfx::Rect bounds(host_window->GetBoundsInScreen().size());

  const gfx::Size buffer_size =
      cc::MathUtil::MapEnclosingClippedRect(window_to_buffer_transform_, bounds)
          .size();

  // Create a single GPU memory buffer. Content will be written into this
  // buffer without any buffering. The result is that we might be modifying
  // the buffer while it's being displayed. This provides minimal latency
  // but with potential tearing. Note that we have to draw into a temporary
  // surface and copy it into GPU memory buffer to avoid flicker.
  gpu_memory_buffer_ = fast_ink_internal::CreateGpuBuffer(
      buffer_size,
      gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                                SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                                             : gfx::BufferFormat::BGRA_8888));
  LOG_IF(ERROR, !gpu_memory_buffer_) << "Failed to create GPU memory buffer";

  if (switches::ShouldClearFastInkBuffer()) {
    bool map_result = gpu_memory_buffer_->Map();
    LOG_IF(ERROR, !map_result) << "Failed to map gpu buffer";
    uint8_t* memory = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    if (memory != nullptr) {
      gfx::Size size = gpu_memory_buffer_->GetSize();
      int stride = gpu_memory_buffer_->stride(0);
      // Clear the buffer before usage, since it may be uninitialized.
      // (http://b/168735625)
      for (int i = 0; i < size.height(); ++i) {
        memset(memory + i * stride, 0, size.width() * 4);
      }
    }
    gpu_memory_buffer_->Unmap();
  }
}

std::unique_ptr<FastInkHost::ScopedPaint> FastInkHost::CreateScopedPaint(
    const gfx::Rect& damage_rect_in_window) const {
  return std::make_unique<ScopedPaint>(this, damage_rect_in_window);
}

std::unique_ptr<viz::CompositorFrame> FastInkHost::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    UiResourceManager& resource_manager,
    bool auto_update,
    const gfx::Size& last_submitted_frame_size,
    float last_submitted_frame_dsf) {
  TRACE_EVENT1("ui", "FastInkHost::SubmitCompositorFrame", "damage",
               GetTotalDamage().ToString());

  auto frame = fast_ink_internal::CreateCompositorFrame(
      begin_frame_ack, GetContentRect(), GetTotalDamage(), auto_update,
      *host_window(), gpu_memory_buffer_.get(), &resource_manager);

  ResetDamage();

  return frame;
}

}  // namespace ash
