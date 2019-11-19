// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/root_frame_sink.h"

#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace android_webview {

namespace {

viz::FrameSinkId AllocateParentSinkId() {
  static base::NoDestructor<viz::FrameSinkIdAllocator> allocator(0u);
  return allocator->NextFrameSinkId();
}

}  // namespace

RootFrameSink::RootFrameSink(SetNeedsBeginFrameCallback set_needs_begin_frame)
    : root_frame_sink_id_(AllocateParentSinkId()),
      set_needs_begin_frame_(set_needs_begin_frame) {
  constexpr bool is_root = true;
  constexpr bool needs_sync_points = true;
  GetFrameSinkManager()->RegisterFrameSinkId(root_frame_sink_id_,
                                             false /* report_activationa */);
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, GetFrameSinkManager(), root_frame_sink_id_, is_root,
      needs_sync_points);
  begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
  GetFrameSinkManager()->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                  root_frame_sink_id_);
}

RootFrameSink::~RootFrameSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetFrameSinkManager()->UnregisterBeginFrameSource(begin_frame_source_.get());
  begin_frame_source_.reset();
  support_.reset();
  GetFrameSinkManager()->InvalidateFrameSinkId(root_frame_sink_id_);
}

viz::FrameSinkManagerImpl* RootFrameSink::GetFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // FrameSinkManagerImpl is global and not owned by this class, which is
  // per-AwContents.
  return VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager();
}

void RootFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReclaimResources(resources);
}

void RootFrameSink::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Root surface should have no resources to return.
  CHECK(resources.empty());
}

void RootFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT_INSTANT1("android_webview", "RootFrameSink::OnNeedsBeginFrames",
                       TRACE_EVENT_SCOPE_THREAD, "needs_begin_frames",
                       needs_begin_frames);
  needs_begin_frames_ = needs_begin_frames;
  set_needs_begin_frame_.Run(needs_begin_frames);
}

void RootFrameSink::AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetFrameSinkManager()->RegisterFrameSinkHierarchy(root_frame_sink_id_,
                                                    frame_sink_id);
}

void RootFrameSink::RemoveChildFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetFrameSinkManager()->UnregisterFrameSinkHierarchy(root_frame_sink_id_,
                                                      frame_sink_id);
}

bool RootFrameSink::BeginFrame(const viz::BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  begin_frame_source_->OnBeginFrame(args);
  // This is a hack and wrong. Remove this once we have proper frame submission
  // tracking.
  return needs_begin_frames_;
}

}  // namespace android_webview
