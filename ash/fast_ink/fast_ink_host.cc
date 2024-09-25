// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/fast_ink/fast_ink_host.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/fast_ink/fast_ink_host_frame_utils.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/video_types.h"

namespace ash {

// -----------------------------------------------------------------------------
// FastInkHost::ScopedPaint

FastInkHost::ScopedPaint::ScopedPaint(const FastInkHost* host,
                                      const gfx::Rect& damage_rect_in_window)
    : host_(const_cast<FastInkHost*>(host)),
      damage_rect_(host->BufferRectFromWindowRect(damage_rect_in_window)),
      canvas_(damage_rect_.size(), /*image_scale*/ 1.0f, /*is_opaque*/ false) {
  canvas_.Translate(-damage_rect_.OffsetFromOrigin());
  canvas_.Transform(host->window_to_buffer_transform_);
}

FastInkHost::ScopedPaint::~ScopedPaint() {
  if (damage_rect_.IsEmpty()) {
    return;
  }
  host_->Draw(canvas_.GetBitmap(), damage_rect_);
}

// -----------------------------------------------------------------------------
// FastInkHost:

FastInkHost::FastInkHost() = default;
FastInkHost::~FastInkHost() {
  if (client_shared_image_) {
    CHECK(context_provider_);
    context_provider_->SharedImageInterface()->DestroySharedImage(
        sync_token_, std::move(client_shared_image_));
  }
}

void FastInkHost::Init(aura::Window* host_window) {
  InitBufferMetadata(host_window);
  FrameSinkHost::Init(host_window);
}

void FastInkHost::InitForTesting(
    aura::Window* host_window,
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  InitBufferMetadata(host_window);
  FrameSinkHost::InitForTesting(host_window, std::move(layer_tree_frame_sink));
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

  CHECK(client_shared_image_);

  auto frame = fast_ink_internal::CreateCompositorFrame(
      begin_frame_ack, GetContentRect(), GetTotalDamage(), auto_update,
      *host_window(), buffer_size_, &resource_manager, client_shared_image_,
      sync_token_);

  ResetDamage();

  return frame;
}

void FastInkHost::OnFirstFrameRequested() {
  InitializeFastInkBuffer(host_window());
}

void FastInkHost::InitBufferMetadata(aura::Window* host_window) {
  // Take the root transform and apply this during buffer update instead of
  // leaving this up to the compositor. The benefit is that HW requirements
  // for being able to take advantage of overlays and direct scanout are
  // reduced significantly. Frames are submitted to the compositor with the
  // inverse transform to cancel out the transformation that would otherwise
  // be done by the compositor.
  window_to_buffer_transform_ = host_window->GetHost()->GetRootTransform();
  gfx::Rect bounds(host_window->GetBoundsInScreen().size());
  // TODO(oshima): Make this eplison default.
  constexpr float kEpsilon = 0.001f;
  buffer_size_ = cc::MathUtil::MapEnclosingClippedRectIgnoringError(
                     window_to_buffer_transform_, bounds, kEpsilon)
                     .size();
}

void FastInkHost::InitializeFastInkBuffer(aura::Window* host_window) {
  // Create a single mappable SharedImage. Content will be written into this
  // SharedImage without any buffering. The result is that we might be modifying
  // the underlying memory while it's being displayed. This provides minimal
  // latency but with potential tearing. Note that to avoid flicker, we draw
  // into a temporary surface and copy it into the mappable SI (see the
  // DrawBitmap() method below).
  context_provider_ = fast_ink_internal::GetContextProvider();
  gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();

  // This SharedImage will be used by the display compositor, will be updated
  // in parallel with being read, and will potentially be used in overlays.
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
      gpu::SHARED_IMAGE_USAGE_SCANOUT;

  CHECK(!client_shared_image_);
  client_shared_image_ = fast_ink_internal::CreateMappableSharedImage(
      buffer_size_, usage, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);

  LOG_IF(ERROR, !client_shared_image_) << "Failed to create MappableSI";
  sync_token_ = sii->GenVerifiedSyncToken();

  if (switches::ShouldClearFastInkBuffer()) {
    std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping;
    if (client_shared_image_) {
      mapping = client_shared_image_->Map();
    }
    LOG_IF(ERROR, !mapping) << "Failed to map MappableSI";
    uint8_t* memory =
        mapping ? static_cast<uint8_t*>(mapping->Memory(0)) : nullptr;
    if (memory != nullptr) {
      gfx::Size size = mapping->Size();
      int stride = mapping->Stride(0);
      // Clear the buffer before usage, since it may be uninitialized.
      // (http://b/168735625)
      for (int i = 0; i < size.height(); ++i) {
        memset(memory + i * stride, 0, size.width() * 4);
      }
    }
  }

  // Draw pending bitmaps to the buffer.
  for (auto pending_bitmap : pending_bitmaps_) {
    DrawBitmap(pending_bitmap.bitmap, pending_bitmap.damage_rect);
  }
  pending_bitmaps_.clear();
}

gfx::Rect FastInkHost::BufferRectFromWindowRect(
    const gfx::Rect& rect_in_window) const {
  return fast_ink_internal::BufferRectFromWindowRect(
      window_to_buffer_transform_, buffer_size_, rect_in_window);
}

void FastInkHost::Draw(SkBitmap bitmap, const gfx::Rect& damage_rect) {
  const bool initialized = client_shared_image_ != nullptr;

  if (!initialized) {
    // GPU process should be ready soon after start and `pending_bitmaps_`
    // should be drawn promptly. 60 is an arbitrary cap that should never
    // hit.
    DCHECK_LT(pending_bitmaps_.size(), 60u);
    pending_bitmaps_.push_back(PendingBitmap(bitmap, damage_rect));
    return;
  }
  DrawBitmap(bitmap, damage_rect);
}

void FastInkHost::DrawBitmap(SkBitmap bitmap, const gfx::Rect& damage_rect) {
  std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping;

  {
    // TODO(zoraiznaeem): Investigate the precision as we will get non trivial
    // additional time of printing the error log.
    TRACE_EVENT0("ui", "FastInkHost::ScopedPaint::Map");

    mapping = client_shared_image_->Map();
    if (!mapping) {
      LOG(ERROR) << "Failed to map MappableSI";
      return;
    }
  }
  // Copy result to the buffer. This is effectively a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "FastInkHost::ScopedPaint::Copy", "damage_rect",
                 damage_rect.ToString());

    uint8_t* data = static_cast<uint8_t*>(mapping->Memory(0));
    const int stride = mapping->Stride(0);
    bitmap.readPixels(
        SkImageInfo::MakeN32Premul(damage_rect.width(), damage_rect.height()),
        data + damage_rect.y() * stride + damage_rect.x() * 4, stride, 0, 0);
  }

  {
    TRACE_EVENT0("ui", "FastInkHost::UpdateBuffer::Unmap");
  }
}

}  // namespace ash
