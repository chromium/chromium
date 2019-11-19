// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_resource_updater.h"
#include "ui/gfx/color_space.h"

namespace cc {

// static
std::unique_ptr<VideoLayerImpl> VideoLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    VideoFrameProvider* provider,
    media::VideoRotation video_rotation) {
  DCHECK(tree_impl->task_runner_provider()->IsMainThreadBlocked());
  DCHECK(tree_impl->task_runner_provider()->IsImplThread());

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl =
      VideoFrameProviderClientImpl::Create(
          provider, tree_impl->GetVideoFrameControllerClient());

  return base::WrapUnique(new VideoLayerImpl(
      tree_impl, id, std::move(provider_client_impl), video_rotation));
}

VideoLayerImpl::VideoLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl,
    media::VideoRotation video_rotation)
    : LayerImpl(tree_impl, id),
      provider_client_impl_(std::move(provider_client_impl)),
      frame_(nullptr),
      video_rotation_(video_rotation) {
  set_may_contain_video(true);
}

VideoLayerImpl::~VideoLayerImpl() {
  if (!provider_client_impl_->Stopped()) {
    // In impl side painting, we may have a pending and active layer
    // associated with the video provider at the same time. Both have a ref
    // on the VideoFrameProviderClientImpl, but we stop when the first
    // LayerImpl (the one on the pending tree) is destroyed since we know
    // the main thread is blocked for this commit.
    DCHECK(layer_tree_impl()->task_runner_provider()->IsImplThread());
    DCHECK(layer_tree_impl()->task_runner_provider()->IsMainThreadBlocked());
    provider_client_impl_->Stop();
  }
}

std::unique_ptr<LayerImpl> VideoLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return base::WrapUnique(new VideoLayerImpl(
      tree_impl, id(), provider_client_impl_, video_rotation_));
}

void VideoLayerImpl::DidBecomeActive() {
  provider_client_impl_->SetActiveVideoLayer(this);
}

bool VideoLayerImpl::WillDraw(DrawMode draw_mode,
                              viz::ClientResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;

  if (!LayerImpl::WillDraw(draw_mode, resource_provider))
    return false;

  // Explicitly acquire and release the provider mutex so it can be held from
  // WillDraw to DidDraw. Since the compositor thread is in the middle of
  // drawing, the layer will not be destroyed before DidDraw is called.
  // Therefore, the only thing that will prevent this lock from being released
  // is the GPU process locking it. As the GPU process can't cause the
  // destruction of the provider (calling StopUsingProvider), holding this
  // lock should not cause a deadlock.
  frame_ = provider_client_impl_->AcquireLockAndCurrentFrame();

  if (!frame_.get()) {
    // Drop any resources used by the updater if there is no frame to display.
    updater_ = nullptr;

    provider_client_impl_->ReleaseLock();
    return false;
  }

  if (!updater_) {
    const LayerTreeSettings& settings = layer_tree_impl()->settings();
    // TODO(sergeyu): Pass RasterContextProvider when it's available. Then
    // remove ContextProvider parameter from VideoResourceUpdater.
    updater_ = std::make_unique<media::VideoResourceUpdater>(
        layer_tree_impl()->context_provider(),
        /*raster_context_provider=*/nullptr,
        layer_tree_impl()->layer_tree_frame_sink(),
        layer_tree_impl()->resource_provider(),
        settings.use_stream_video_draw_quad,
        settings.resource_settings.use_gpu_memory_buffer_resources,
        settings.resource_settings.use_r16_texture,
        layer_tree_impl()->max_texture_size());
  }
  updater_->ObtainFrameResources(frame_);
  return true;
}

void VideoLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                 AppendQuadsData* append_quads_data) {
  DCHECK(frame_.get());

  gfx::Transform transform = DrawTransform();
  // bounds() is in post-rotation space so quad rect in content space must be
  // in pre-rotation space
  gfx::Size rotated_size = bounds();

  switch (video_rotation_) {
    case media::VIDEO_ROTATION_90:
      rotated_size = gfx::Size(rotated_size.height(), rotated_size.width());
      transform.Rotate(90.0);
      transform.Translate(0.0, -rotated_size.height());
      break;
    case media::VIDEO_ROTATION_180:
      transform.Rotate(180.0);
      transform.Translate(-rotated_size.width(), -rotated_size.height());
      break;
    case media::VIDEO_ROTATION_270:
      rotated_size = gfx::Size(rotated_size.height(), rotated_size.width());
      transform.Rotate(270.0);
      transform.Translate(-rotated_size.width(), 0);
      break;
    case media::VIDEO_ROTATION_0:
      break;
  }

  gfx::Rect quad_rect(rotated_size);
  Occlusion occlusion_in_video_space =
      draw_properties()
          .occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
              transform);
  gfx::Rect visible_quad_rect =
      occlusion_in_video_space.GetUnoccludedContentRect(quad_rect);
  if (visible_quad_rect.IsEmpty())
    return;

  updater_->AppendQuads(
      render_pass, frame_, transform, quad_rect, visible_quad_rect,
      draw_properties().rounded_corner_bounds, clip_rect(), is_clipped(),
      contents_opaque(), draw_opacity(), GetSortingContextId());
}

void VideoLayerImpl::DidDraw(viz::ClientResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  DCHECK(frame_.get());

  updater_->ReleaseFrameResources();
  provider_client_impl_->PutCurrentFrame();
  frame_ = nullptr;

  provider_client_impl_->ReleaseLock();
}

SimpleEnclosedRegion VideoLayerImpl::VisibleOpaqueRegion() const {
  // If we don't have a frame yet, then we don't have an opaque region.
  if (!provider_client_impl_->HasCurrentFrame())
    return SimpleEnclosedRegion();
  return LayerImpl::VisibleOpaqueRegion();
}

void VideoLayerImpl::ReleaseResources() {
  updater_ = nullptr;
}

void VideoLayerImpl::SetNeedsRedraw() {
  UnionUpdateRect(gfx::Rect(bounds()));
  layer_tree_impl()->SetNeedsRedraw();
}

const char* VideoLayerImpl::LayerTypeAsString() const {
  return "cc::VideoLayerImpl";
}

}  // namespace cc
