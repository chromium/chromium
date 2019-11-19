// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/child_frame.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace android_webview {

ChildFrame::ChildFrame(
    scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::Size& viewport_size_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority,
    bool offscreen_pre_raster,
    float device_scale_factor,
    CopyOutputRequestQueue copy_requests)
    : frame_future(std::move(frame_future)),
      frame_sink_id(frame_sink_id),
      viewport_size_for_tile_priority(viewport_size_for_tile_priority),
      transform_for_tile_priority(transform_for_tile_priority),
      offscreen_pre_raster(offscreen_pre_raster),
      device_scale_factor(device_scale_factor),
      copy_requests(std::move(copy_requests)) {}

ChildFrame::~ChildFrame() {
}

void ChildFrame::WaitOnFutureIfNeeded() {
  if (!frame_future)
    return;

  TRACE_EVENT0("android_webview", "GetFrame");
  DCHECK(!frame);
  auto frame_ptr = frame_future->GetFrame();
  if (frame_ptr) {
    layer_tree_frame_sink_id = frame_ptr->layer_tree_frame_sink_id;
    frame = std::move(frame_ptr->frame);
    local_surface_id = frame_future->local_surface_id();
  }
  frame_future = nullptr;
}

viz::SurfaceId ChildFrame::GetSurfaceId() const {
  DCHECK(!frame_future);
  return viz::SurfaceId(frame_sink_id, local_surface_id);
}

}  // namespace android_webview
