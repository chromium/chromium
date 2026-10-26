// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/camera_video_frame_renderer.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/frame_sink/frame_sink_utils.h"
#include "base/check.h"
#include "base/check_op.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_id.h"
#include "media/base/video_frame.h"
#include "media/gfx/video_resource_updater.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer_surface.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ash {

namespace {

ui::ContextFactory* GetContextFactory() {
  return aura::Env::GetInstance()->context_factory();
}

}  // namespace

CameraVideoFrameRenderer::CameraVideoFrameRenderer(
    mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
    const media::VideoCaptureFormat& capture_format,
    bool should_flip_frames_horizontally)
    : host_window_(/*delegate=*/nullptr),
      video_frame_handler_(GetContextFactory(),
                           std::move(camera_video_source),
                           capture_format),
      should_flip_frames_horizontally_(should_flip_frames_horizontally) {
  host_window_.set_owned_by_parent(false);
  host_window_.Init(ui::LAYER_SURFACE);
  host_window_.layer()->AsSurface()->SetFallbackBackgroundColor(
      SkColors::kDkGray);
  host_window_.SetName("CameraVideoFramesHost");
}

CameraVideoFrameRenderer::~CameraVideoFrameRenderer() = default;

void CameraVideoFrameRenderer::Init(aura::Window* host_window) {
  CHECK_EQ(host_window, &host_window_);
  FrameSinkHost::Init(host_window);
  video_frame_handler_.StartHandlingFrames(/*delegate=*/this);
}

void CameraVideoFrameRenderer::OnCameraVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  current_video_frame_ = std::move(frame);
  const gfx::Rect rect(host_window_.bounds().size());
  UpdateSurface(rect, rect, /*synchronous_draw=*/false);
}

void CameraVideoFrameRenderer::OnFatalErrorOrDisconnection() {
  CaptureModeController::Get()->camera_controller()->OnFrameHandlerFatalError();
  // `this` will be deleted soon after the above call. "Soon" here because the
  // `camera_preview_widget_` which indirectly owns `this` is destroyed
  // asynchronously when `Close()` is called on it.
}

std::unique_ptr<viz::CompositorFrame>
CameraVideoFrameRenderer::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    viz::ClientResourceProvider& client_resource_provider,
    cc::ResourcePool& /*resource_pool*/,
    bool auto_update) {
  if (!current_video_frame_) {
    return nullptr;
  }

  scoped_refptr<media::VideoFrame> video_frame =
      std::move(current_video_frame_);

  MaybeCreateVideoResourceUpdater(client_resource_provider);

  auto compositor_frame = std::make_unique<viz::CompositorFrame>();
  compositor_frame->metadata.begin_frame_ack = begin_frame_ack;
  compositor_frame->metadata.begin_frame_ack.has_damage = true;
  compositor_frame->metadata.may_contain_video = true;
  const float dsf = host_window_.GetHost()->device_scale_factor();
  compositor_frame->metadata.device_scale_factor = dsf;

  // In our current implementation the `host_window_` is always expected to be
  // a square.
  const gfx::Size host_size_pixels = gfx::ToRoundedSize(
      gfx::ConvertSizeToPixels(host_window_.bounds().size(), dsf));
  DCHECK_EQ(host_size_pixels.width(), host_size_pixels.height());

  // Calculate the compositor frame size based on the video frame size, such
  // that the video frame is scaled to fit the size of the `host_window_` in
  // pixels.
  const gfx::Size video_frame_size = video_frame->natural_size();
  gfx::Size compositor_frame_size;
  // Pick the smallest side of the video frame and scale it to fit the
  // corresponding length of the `host_window_`. Scale the other side
  // maintaining the same aspect ratio. This ensures that we don't end up with
  // a compositor frame with one side shorter than the corresponding side of the
  // `host_window_`.
  if (video_frame_size.height() <= video_frame_size.width()) {
    const float scale = static_cast<float>(host_size_pixels.height()) /
                        video_frame_size.height();
    compositor_frame_size.SetSize(std::ceilf(video_frame_size.width() * scale),
                                  host_size_pixels.height());
  } else {
    const float scale =
        static_cast<float>(host_size_pixels.width()) / video_frame_size.width();
    compositor_frame_size.SetSize(
        host_size_pixels.width(),
        std::ceilf(video_frame_size.height() * scale));
  }
  DCHECK(!compositor_frame_size.IsEmpty());
  DCHECK(compositor_frame_size.width() == host_size_pixels.width() ||
         compositor_frame_size.height() == host_size_pixels.height());

  const gfx::Rect quad_rect(compositor_frame_size);
  auto render_pass =
      viz::CompositorRenderPass::Create(/*shared_quad_state_list_size=*/1u,
                                        /*quad_list_size=*/1u);
  render_pass->SetNew(viz::CompositorRenderPassId{1}, quad_rect, quad_rect,
                      gfx::Transform());

  // If the camera should behave like a mirror, we should flip the frames around
  // the Y axis (by scaling by -1 in X).
  gfx::Transform transform;
  if (should_flip_frames_horizontally_) {
    transform.Scale(-1, 1);
    transform.Translate(-compositor_frame_size.width(), 0);
  }

  // Center the compositor frame horizontally and vertically inside
  // `host_window_`.
  int x_offset =
      -(compositor_frame_size.width() - host_size_pixels.width()) / 2;
  const int y_offset =
      -(compositor_frame_size.height() - host_size_pixels.height()) / 2;

  // When the frame is flipped horizontally around Y, we offset in the opposite
  // direction.
  if (should_flip_frames_horizontally_)
    x_offset *= -1;

  transform.Translate(x_offset, y_offset);

  video_resource_updater_->ObtainFrameResource(video_frame);

  const bool context_opaque = media::IsOpaque(video_frame->format());
  video_resource_updater_->AppendQuad(
      render_pass.get(), video_frame, transform, quad_rect,
      /*visible_quad_rect=*/quad_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/std::nullopt, context_opaque,
      /*draw_opacity=*/1.0f, /*sorting_context_id=*/0);
  compositor_frame->render_pass_list.emplace_back(std::move(render_pass));

  const auto& quad_list = compositor_frame->render_pass_list.back()->quad_list;
  DCHECK_EQ(quad_list.size(), 1u);

  auto resource_id = quad_list.front()->resource_id;
  if (resource_id != viz::kInvalidResourceId) {
    std::vector<viz::TransferableResource> resource_list;
    CHECK(context_provider_);
    client_resource_provider.PrepareSendToParent(
        {resource_id}, &resource_list,
        context_provider_->SharedImageInterface());
    compositor_frame->resource_list = std::move(resource_list);
  }

  video_resource_updater_->ReleaseFrameResource();

  if (on_video_frame_rendered_for_test_) {
    std::move(on_video_frame_rendered_for_test_).Run(std::move(video_frame));
  }

  return compositor_frame;
}

void CameraVideoFrameRenderer::OnFrameSinkLost() {
  // The base class replaces the FrameSinkHolder whose ClientResourceProvider
  // the updater uses (and the GPU context is likely lost too, e.g. GPU crash),
  // so recreate the updater with the next frame.
  video_resource_updater_.reset();
  video_resource_provider_ = nullptr;
  FrameSinkHost::OnFrameSinkLost();
}

void CameraVideoFrameRenderer::MaybeCreateVideoResourceUpdater(
    viz::ClientResourceProvider& client_resource_provider) {
  if (video_resource_updater_) {
    CHECK_EQ(video_resource_provider_.get(), &client_resource_provider);
    return;
  }

  // Created lazily because it must use the current FrameSinkHolder's
  // `client_resource_provider`. No SharedImageInterface is passed since it is
  // only used for software compositing.
  context_provider_ = frame_sink_utils::GetContextProvider();
  CHECK(context_provider_);
  video_resource_provider_ = &client_resource_provider;
  video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
      context_provider_.get(), &client_resource_provider,
      /*shared_image_interface=*/nullptr,
      /*use_gpu_memory_buffer_resources=*/false,
      context_provider_->ContextCapabilities().max_texture_size);
}

}  // namespace ash
