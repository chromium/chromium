// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/hardware_renderer_single_thread.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/surfaces_instance.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/transform.h"

namespace android_webview {

HardwareRendererSingleThread::HardwareRendererSingleThread(
    RenderThreadManager* state)
    : HardwareRenderer(state),
      surfaces_(SurfacesInstance::GetOrCreateInstance()),
      frame_sink_id_(surfaces_->AllocateFrameSinkId()),
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()),
      last_submitted_layer_tree_frame_sink_id_(0u) {
  surfaces_->GetFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, true /* report_activation */);
  surfaces_->GetFrameSinkManager()->SetFrameSinkDebugLabel(
      frame_sink_id_, "HardwareRendererSingleThread");
  CreateNewCompositorFrameSinkSupport();
}

HardwareRendererSingleThread::~HardwareRendererSingleThread() {
  // Must reset everything before |surface_factory_| to ensure all
  // resources are returned before resetting.
  if (child_id_.is_valid())
    DestroySurface();
  support_.reset();
  surfaces_->GetFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
}

void HardwareRendererSingleThread::DrawAndSwap(
    HardwareRendererDrawParams* params) {
  TRACE_EVENT1("android_webview", "HardwareRendererSingleThread::DrawAndSwap",
               "vulkan", surfaces_->is_using_vulkan());

  bool submitted_new_frame = false;
  uint32_t frame_token = 0u;
  // SurfaceFactory::SubmitCompositorFrame might call glFlush. So calling it
  // during "kModeSync" stage (which does not allow GL) might result in extra
  // kModeProcess. Instead, submit the frame in "kModeDraw" stage to avoid
  // unnecessary kModeProcess.
  if (child_frame_.get() && child_frame_->frame.get()) {
    if (child_frame_sink_id_ != child_frame_->frame_sink_id ||
        last_submitted_layer_tree_frame_sink_id_ !=
            child_frame_->layer_tree_frame_sink_id) {
      if (child_id_.is_valid())
        DestroySurface();

      CreateNewCompositorFrameSinkSupport();
      child_frame_sink_id_ = child_frame_->frame_sink_id;
      last_submitted_layer_tree_frame_sink_id_ =
          child_frame_->layer_tree_frame_sink_id;
    }

    std::unique_ptr<viz::CompositorFrame> child_compositor_frame =
        std::move(child_frame_->frame);

    float device_scale_factor = child_compositor_frame->device_scale_factor();
    gfx::Size frame_size = child_compositor_frame->size_in_pixels();
    if (!child_id_.is_valid() || surface_size_ != frame_size ||
        device_scale_factor_ != device_scale_factor) {
      if (child_id_.is_valid())
        DestroySurface();
      AllocateSurface();
      surface_size_ = frame_size;
      device_scale_factor_ = device_scale_factor;
    }

    frame_token = child_compositor_frame->metadata.frame_token;
    support_->SubmitCompositorFrame(child_id_,
                                    std::move(*child_compositor_frame));
    submitted_new_frame = true;
  }

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(params->transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(params->width, params->height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints(viewport, transform);
  bool need_to_update_draw_constraints =
      !child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_);

  // Post data after draw if submitted_new_frame, since we may have
  // presentation feedback to return as well.
  if (need_to_update_draw_constraints && !submitted_new_frame) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, child_frame_sink_id_, viz::FrameTimingDetailsMap(),
        0u);
  }

  if (!child_id_.is_valid())
    return;

  CopyOutputRequestQueue requests;
  requests.swap(child_frame_->copy_requests);
  for (auto& copy_request : requests) {
    support_->RequestCopyOfOutput(child_id_, std::move(copy_request));
  }

  gfx::Rect clip(params->clip_left, params->clip_top,
                 params->clip_right - params->clip_left,
                 params->clip_bottom - params->clip_top);
  surfaces_->DrawAndSwap(viewport, clip, transform, surface_size_,
                         viz::SurfaceId(frame_sink_id_, child_id_),
                         device_scale_factor_, params->color_space);
  viz::FrameTimingDetailsMap timing_details =
      support_->TakeFrameTimingDetailsMap();
  if (submitted_new_frame) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, child_frame_sink_id_, std::move(timing_details),
        frame_token);
  }
}

void HardwareRendererSingleThread::AllocateSurface() {
  DCHECK(!child_id_.is_valid());
  parent_local_surface_id_allocator_->GenerateId();
  child_id_ =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id();
  surfaces_->AddChildId(viz::SurfaceId(frame_sink_id_, child_id_));
}

void HardwareRendererSingleThread::DestroySurface() {
  DCHECK(child_id_.is_valid());

  surfaces_->RemoveChildId(viz::SurfaceId(frame_sink_id_, child_id_));
  support_->EvictSurface(child_id_);
  child_id_ = viz::LocalSurfaceId();
  surfaces_->GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
}

void HardwareRendererSingleThread::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, child_frame_sink_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRendererSingleThread::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  NOTREACHED();
}

void HardwareRendererSingleThread::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, child_frame_sink_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRendererSingleThread::OnBeginFramePausedChanged(bool paused) {}

void HardwareRendererSingleThread::CreateNewCompositorFrameSinkSupport() {
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, surfaces_->GetFrameSinkManager(), frame_sink_id_, is_root,
      needs_sync_points);
}

}  // namespace android_webview
