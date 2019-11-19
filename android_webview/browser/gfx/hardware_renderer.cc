// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/hardware_renderer.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRenderer::HardwareRenderer(RenderThreadManager* state)
    : render_thread_manager_(state),
      last_egl_context_(eglGetCurrentContext()) {}

HardwareRenderer::~HardwareRenderer() {
  // Reset draw constraints.
  if (child_frame_) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        ParentCompositorDrawConstraints(), child_frame_->frame_sink_id,
        viz::FrameTimingDetailsMap(), 0u);
  }
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

void HardwareRenderer::Draw(HardwareRendererDrawParams* params) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::Draw");

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

  if (last_egl_context_) {
    // We need to watch if the current Android context has changed and enforce a
    // clean-up in the compositor.
    EGLContext current_context = eglGetCurrentContext();
    DCHECK(current_context) << "Draw called without EGLContext";

    // TODO(boliu): Handle context loss.
    if (last_egl_context_ != current_context)
      DLOG(WARNING) << "EGLContextChanged";
  }

  DrawAndSwap(params);
}

void HardwareRenderer::ReturnChildFrame(
    std::unique_ptr<ChildFrame> child_frame) {
  if (!child_frame || !child_frame->frame)
    return;

  std::vector<viz::ReturnedResource> resources_to_return =
      viz::TransferableResource::ReturnResources(
          child_frame->frame->resource_list);

  // The child frame's frame_sink_id is not necessarily same as
  // |child_frame_sink_id_|.
  ReturnResourcesToCompositor(resources_to_return, child_frame->frame_sink_id,
                              child_frame->layer_tree_frame_sink_id);
}

void HardwareRenderer::ReturnResourcesToCompositor(
    const std::vector<viz::ReturnedResource>& resources,
    const viz::FrameSinkId& frame_sink_id,
    uint32_t layer_tree_frame_sink_id) {
  if (layer_tree_frame_sink_id != last_committed_layer_tree_frame_sink_id_)
    return;
  render_thread_manager_->InsertReturnedResourcesOnRT(resources, frame_sink_id,
                                                      layer_tree_frame_sink_id);
}

namespace {

void MoveCopyRequests(CopyOutputRequestQueue* from,
                      CopyOutputRequestQueue* to) {
  std::move(from->begin(), from->end(), std::back_inserter(*to));
  from->clear();
}

}  // namespace

// static
ChildFrameQueue HardwareRenderer::WaitAndPruneFrameQueue(
    ChildFrameQueue* child_frames_ptr) {
  ChildFrameQueue& child_frames = *child_frames_ptr;
  ChildFrameQueue pruned_frames;
  if (child_frames.empty())
    return pruned_frames;

  // First find the last non-empty frame.
  int remaining_frame_index = -1;
  for (size_t i = 0; i < child_frames.size(); ++i) {
    auto& child_frame = *child_frames[i];
    child_frame.WaitOnFutureIfNeeded();
    if (child_frame.frame)
      remaining_frame_index = i;
  }
  // If all empty, keep the last one.
  if (remaining_frame_index < 0)
    remaining_frame_index = child_frames.size() - 1;

  // Prune end.
  while (child_frames.size() > static_cast<size_t>(remaining_frame_index + 1)) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.back());
    child_frames.pop_back();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames[remaining_frame_index]->copy_requests);
    DCHECK(!frame->frame);
  }
  DCHECK_EQ(static_cast<size_t>(remaining_frame_index),
            child_frames.size() - 1);

  // Prune front.
  while (child_frames.size() > 1) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.front());
    child_frames.pop_front();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames.back()->copy_requests);
    if (frame->frame)
      pruned_frames.emplace_back(std::move(frame));
  }
  return pruned_frames;
}

}  // namespace android_webview
