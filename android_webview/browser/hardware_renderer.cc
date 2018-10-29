// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer.h"

#include <memory>
#include <utility>

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/aw_render_thread_context_provider.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/browser/surfaces_instance.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRenderer::HardwareRenderer(RenderThreadManager* state)
    : render_thread_manager_(state),
      last_egl_context_(eglGetCurrentContext()),
      surfaces_(SurfacesInstance::GetOrCreateInstance()),
      frame_sink_id_(surfaces_->AllocateFrameSinkId()),
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()),
      last_committed_layer_tree_frame_sink_id_(0u),
      last_submitted_layer_tree_frame_sink_id_(0u) {
  DCHECK(last_egl_context_);
  surfaces_->GetFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, true /* report_activation */);
  surfaces_->GetFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                           "HardwareRenderer");
  CreateNewCompositorFrameSinkSupport();
}

HardwareRenderer::~HardwareRenderer() {
  // Must reset everything before |surface_factory_| to ensure all
  // resources are returned before resetting.
  if (child_id_.is_valid())
    DestroySurface();
  support_.reset();
  surfaces_->GetFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);

  // Reset draw constraints.
  render_thread_manager_->PostExternalDrawConstraintsToChildCompositorOnRT(
      ParentCompositorDrawConstraints());
  for (auto& child_frame : child_frame_queue_) {
    child_frame->WaitOnFutureIfNeeded();
    ReturnChildFrame(std::move(child_frame));
  }
}

void HardwareRenderer::CommitFrame() {
  TRACE_EVENT0("android_webview", "CommitFrame");
  scroll_offset_ = render_thread_manager_->GetScrollOffsetOnRT();
  ChildFrameQueue child_frames = render_thread_manager_->PassFramesOnRT();
  // |child_frames| should have at most one non-empty frame, and one current
  // and unwaited frame, in that order.
  DCHECK_LE(child_frames.size(), 2u);
  if (child_frames.empty())
    return;
  // Insert all except last, ie current frame.
  while (child_frames.size() > 1u) {
    child_frame_queue_.emplace_back(std::move(child_frames.front()));
    child_frames.pop_front();
  }
  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_))
    ReturnChildFrame(std::move(pruned_frame));
  DCHECK_LE(child_frame_queue_.size(), 1u);
  child_frame_queue_.emplace_back(std::move(child_frames.front()));
}

void HardwareRenderer::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::DrawGL");

  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_))
    ReturnChildFrame(std::move(pruned_frame));
  DCHECK_LE(child_frame_queue_.size(), 1u);
  if (!child_frame_queue_.empty()) {
    child_frame_ = std::move(child_frame_queue_.front());
    child_frame_queue_.clear();
  }
  if (child_frame_) {
    last_committed_layer_tree_frame_sink_id_ =
        child_frame_->layer_tree_frame_sink_id;
  }

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  DCHECK(current_context) << "DrawGL called without EGLContext";

  // TODO(boliu): Handle context loss.
  if (last_egl_context_ != current_context)
    DLOG(WARNING) << "EGLContextChanged";

  // SurfaceFactory::SubmitCompositorFrame might call glFlush. So calling it
  // during "kModeSync" stage (which does not allow GL) might result in extra
  // kModeProcess. Instead, submit the frame in "kModeDraw" stage to avoid
  // unnecessary kModeProcess.
  if (child_frame_.get() && child_frame_->frame.get()) {
    if (!compositor_id_.Equals(child_frame_->compositor_id) ||
        last_submitted_layer_tree_frame_sink_id_ !=
            child_frame_->layer_tree_frame_sink_id) {
      if (child_id_.is_valid())
        DestroySurface();

      CreateNewCompositorFrameSinkSupport();
      compositor_id_ = child_frame_->compositor_id;
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

    support_->SubmitCompositorFrame(child_id_,
                                    std::move(*child_compositor_frame));
  }

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(draw_info->width, draw_info->height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints(
      draw_info->is_layer, transform, viewport.IsEmpty());
  if (!child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_)) {
    render_thread_manager_->PostExternalDrawConstraintsToChildCompositorOnRT(
        draw_constraints);
  }

  if (!child_id_.is_valid())
    return;

  gfx::Rect clip(draw_info->clip_left, draw_info->clip_top,
                 draw_info->clip_right - draw_info->clip_left,
                 draw_info->clip_bottom - draw_info->clip_top);
  surfaces_->DrawAndSwap(viewport, clip, transform, surface_size_,
                         viz::SurfaceId(frame_sink_id_, child_id_),
                         device_scale_factor_);
}

void HardwareRenderer::AllocateSurface() {
  DCHECK(!child_id_.is_valid());
  child_id_ = parent_local_surface_id_allocator_->GenerateId();
  surfaces_->AddChildId(viz::SurfaceId(frame_sink_id_, child_id_));
}

void HardwareRenderer::DestroySurface() {
  DCHECK(child_id_.is_valid());

  surfaces_->RemoveChildId(viz::SurfaceId(frame_sink_id_, child_id_));
  support_->EvictSurface(child_id_);
  child_id_ = viz::LocalSurfaceId();
  surfaces_->GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
}

void HardwareRenderer::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, compositor_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRenderer::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {}

void HardwareRenderer::OnBeginFrame(const viz::BeginFrameArgs& args) {
  // TODO(tansell): Hook this up.
}

void HardwareRenderer::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, compositor_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRenderer::OnBeginFramePausedChanged(bool paused) {}

// static
ChildFrameQueue HardwareRenderer::WaitAndPruneFrameQueue(
    ChildFrameQueue* child_frames_ptr) {
  ChildFrameQueue& child_frames = *child_frames_ptr;
  ChildFrameQueue pruned_frames;

  // First find the last non-empty frame.
  int last_non_empty_index = -1;
  for (size_t i = 0; i < child_frames.size(); ++i) {
    auto& child_frame = *child_frames[i];
    child_frame.WaitOnFutureIfNeeded();
    if (child_frame.frame)
      last_non_empty_index = i;
  }
  if (last_non_empty_index < 0) {
    child_frames.clear();
    return pruned_frames;
  }

  // Prune end.
  while (child_frames.size() > static_cast<size_t>(last_non_empty_index + 1)) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.back());
    child_frames.pop_back();
    if (frame->frame)
      pruned_frames.emplace_back(std::move(frame));
  }

  // Prune front.
  while (child_frames.size() > 1) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.front());
    child_frames.pop_front();
    if (frame->frame)
      pruned_frames.emplace_back(std::move(frame));
  }
  return pruned_frames;
}

void HardwareRenderer::ReturnChildFrame(
    std::unique_ptr<ChildFrame> child_frame) {
  if (!child_frame || !child_frame->frame)
    return;

  std::vector<viz::ReturnedResource> resources_to_return =
      viz::TransferableResource::ReturnResources(
          child_frame->frame->resource_list);

  // The child frame's compositor id is not necessarily same as
  // compositor_id_.
  ReturnResourcesToCompositor(resources_to_return, child_frame->compositor_id,
                              child_frame->layer_tree_frame_sink_id);
}

void HardwareRenderer::ReturnResourcesToCompositor(
    const std::vector<viz::ReturnedResource>& resources,
    const CompositorID& compositor_id,
    uint32_t layer_tree_frame_sink_id) {
  if (layer_tree_frame_sink_id != last_committed_layer_tree_frame_sink_id_)
    return;
  render_thread_manager_->InsertReturnedResourcesOnRT(resources, compositor_id,
                                                      layer_tree_frame_sink_id);
}

void HardwareRenderer::CreateNewCompositorFrameSinkSupport() {
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, surfaces_->GetFrameSinkManager(), frame_sink_id_, is_root,
      needs_sync_points);
}

}  // namespace android_webview
