// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/capture_mode/camera_video_frame_renderer.h"

#include <cmath>
#include <iterator>
#include <memory>
#include <vector>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "base/check.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "media/renderers/video_resource_updater.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
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
      context_provider_(
          GetContextFactory()->SharedMainThreadRasterContextProvider()),
      should_flip_frames_horizontally_(should_flip_frames_horizontally) {
  host_window_.set_owned_by_parent(false);
  host_window_.Init(ui::LAYER_SOLID_COLOR);
  host_window_.layer()->SetColor(SK_ColorDKGRAY);
  host_window_.SetName("CameraVideoFramesHost");
}

CameraVideoFrameRenderer::~CameraVideoFrameRenderer() {
  if (layer_tree_frame_sink_)
    layer_tree_frame_sink_->DetachFromClient();
  client_resource_provider_.ShutdownAndReleaseAllResources();
}

void CameraVideoFrameRenderer::Initialize() {
  DCHECK(!layer_tree_frame_sink_);
  DCHECK(host_window_.parent())
      << "Before calling Initialize(), host_window_ must be added to the "
         "window hierarchy first.";

  layer_tree_frame_sink_ = host_window_.CreateLayerTreeFrameSink();
  layer_tree_frame_sink_->BindToClient(this);

  const int max_texture_size =
      context_provider_->ContextCapabilities().max_texture_size;
  video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
      context_provider_.get(), layer_tree_frame_sink_.get(),
      &client_resource_provider_,
      layer_tree_frame_sink_->shared_image_interface(),
      /*use_stream_video_draw_quad=*/false,
      /*use_gpu_memory_buffer_resources=*/false, max_texture_size);

  video_frame_handler_.StartHandlingFrames(/*delegate=*/this);
}

void CameraVideoFrameRenderer::OnCameraVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(layer_tree_frame_sink_);
  DCHECK(video_resource_updater_);

  current_video_frame_ = std::move(frame);
}

void CameraVideoFrameRenderer::OnFatalErrorOrDisconnection() {
  CaptureModeController::Get()->camera_controller()->OnFrameHandlerFatalError();
  // `this` will be deleted soon after the above call. "Soon" here because the
  // `camera_preview_widget_` which indirectly owns `this` is destroyed
  // asynchronously when `Close()` is called on it.
}

void CameraVideoFrameRenderer::OnBeginFrameSourcePausedChanged(bool paused) {}

bool CameraVideoFrameRenderer::OnBeginFrameDerivedImpl(
    const viz::BeginFrameArgs& args) {
  viz::BeginFrameAck current_begin_frame_ack(args, false);
  if (pending_compositor_frame_ack_ || !current_video_frame_) {
    // TODO(afakhry): If latency becomes an issue, instead of calling
    // `DidNotProduceFrame()` immediately, we can wait within a deadline for
    // a new video frame, and call `DidNotProduceFrame()` if the deadline
    // expires and no video frame was received.
    layer_tree_frame_sink_->DidNotProduceFrame(
        current_begin_frame_ack, cc::FrameSkippedReason::kWaitingOnMain);
    return false;
  }

  // We move the `current_video_frame_` below into `CreateCompositorFrame()`.
  // However, `on_video_frame_rendered_for_test_` (if valid) should be called
  // after the compositor frame has been submitted, so we extend the lifetime of
  // frame by keeping ref-counted ptr to it here.
  scoped_refptr<media::VideoFrame> frame_for_test;
  if (on_video_frame_rendered_for_test_)
    frame_for_test = current_video_frame_;

  pending_compositor_frame_ack_ = true;
  video_resource_updater_->ObtainFrameResource(current_video_frame_);
  layer_tree_frame_sink_->SubmitCompositorFrame(
      CreateCompositorFrame(current_begin_frame_ack,
                            std::move(current_video_frame_)),
      /*hit_test_data_changed=*/false);
  video_resource_updater_->ReleaseFrameResource();

  if (on_video_frame_rendered_for_test_) {
    DCHECK(frame_for_test);
    std::move(on_video_frame_rendered_for_test_).Run(std::move(frame_for_test));
  }

  return true;
}

void CameraVideoFrameRenderer::SetBeginFrameSource(
    viz::BeginFrameSource* source) {
  if (source == begin_frame_source_)
    return;

  if (begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = source;
  if (begin_frame_source_)
    begin_frame_source_->AddObserver(this);
}

std::optional<viz::HitTestRegionList>
CameraVideoFrameRenderer::BuildHitTestData() {
  return std::nullopt;
}

void CameraVideoFrameRenderer::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  client_resource_provider_.ReceiveReturnsFromParent(std::move(resources));
}

void CameraVideoFrameRenderer::SetTreeActivationCallback(
    base::RepeatingClosure callback) {}

void CameraVideoFrameRenderer::DidReceiveCompositorFrameAck() {
  pending_compositor_frame_ack_ = false;
}

void CameraVideoFrameRenderer::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {}

void CameraVideoFrameRenderer::DidLoseLayerTreeFrameSink() {}

void CameraVideoFrameRenderer::OnDraw(const gfx::Transform& transform,
                                      const gfx::Rect& viewport,
                                      bool resourceless_software_draw,
                                      bool skip_draw) {}

void CameraVideoFrameRenderer::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

void CameraVideoFrameRenderer::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {}

viz::CompositorFrame CameraVideoFrameRenderer::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame) {
  viz::CompositorFrame compositor_frame;
  compositor_frame.metadata.frame_token = ++compositor_frame_token_generator_;
  compositor_frame.metadata.begin_frame_ack = begin_frame_ack;
  if (video_frame->ColorSpace().IsValid()) {
    compositor_frame.metadata.content_color_usage =
        video_frame->ColorSpace().GetContentColorUsage();
  }
  compositor_frame.metadata.begin_frame_ack.has_damage = true;
  compositor_frame.metadata.may_contain_video = true;
  const float dsf = host_window_.GetHost()->device_scale_factor();
  compositor_frame.metadata.device_scale_factor = dsf;

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

  // A change in the size of the compositor frame means we need to identify a
  // new surface to submit the compositor frame to since the surface size is
  // different.
  if (compositor_frame_size != last_compositor_frame_size_pixels_ ||
      dsf != last_compositor_frame_dsf_) {
    last_compositor_frame_size_pixels_ = compositor_frame_size;
    last_compositor_frame_dsf_ = dsf;
    host_window_.AllocateLocalSurfaceId();
  }

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

  const bool context_opaque = media::IsOpaque(video_frame->format());
  // Note that `video_frame`'s ownership is moved into `AppendQuad()`. Do not
  // access after the `std::move()` below.
  video_resource_updater_->AppendQuad(
      render_pass.get(), std::move(video_frame), transform, quad_rect,
      /*visible_quad_rect=*/quad_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/std::nullopt, context_opaque,
      /*draw_opacity=*/1.0f, /*sorting_context_id=*/0);
  compositor_frame.render_pass_list.emplace_back(std::move(render_pass));

  const auto& quad_list = compositor_frame.render_pass_list.back()->quad_list;
  DCHECK_EQ(quad_list.size(), 1u);
  const viz::DrawQuad::Resources& resources = quad_list.front()->resources;
  std::vector<viz::ResourceId> resource_ids;
  resource_ids.reserve(resources.count);
  for (uint32_t i = 0; i < resources.count; ++i)
    resource_ids.push_back(resources.ids[i]);

  std::vector<viz::TransferableResource> resource_list;
  client_resource_provider_.PrepareSendToParent(resource_ids, &resource_list,
                                                context_provider_.get());
  compositor_frame.resource_list = std::move(resource_list);

  return compositor_frame;
}

}  // namespace ash
