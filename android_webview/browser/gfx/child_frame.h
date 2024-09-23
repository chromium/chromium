// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/threading/platform_thread.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
class CompositorFrame;
class CopyOutputRequest;
}

namespace android_webview {

using CopyOutputRequestQueue =
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>;

// Lifetime: WebView
class ChildFrame {
 public:
  ChildFrame(
      scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& viewport_size_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority,
      bool offscreen_pre_raster,
      float device_scale_factor,
      CopyOutputRequestQueue copy_requests,
      bool did_invalidate,
      const viz::BeginFrameArgs& begin_frame_args,
      const base::flat_set<base::PlatformThreadId>& renderer_thread_ids,
      base::PlatformThreadId browser_io_thread_id);

  ChildFrame(const ChildFrame&) = delete;
  ChildFrame& operator=(const ChildFrame&) = delete;

  ~ChildFrame();

  // Helper to move frame from |frame_future| to |frame|.
  void WaitOnFutureIfNeeded();
  viz::SurfaceId GetSurfaceId() const;

  // The frame is either in |frame_future| or |frame|. It's illegal if both
  // are non-null.
  scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future;
  uint32_t layer_tree_frame_sink_id = 0u;
  std::unique_ptr<viz::CompositorFrame> frame;
  std::optional<viz::HitTestRegionList> hit_test_region_list;
  // The id of the compositor this |frame| comes from.
  const viz::FrameSinkId frame_sink_id;
  // Local surface id of the frame. Invalid if |frame| is null.
  viz::LocalSurfaceId local_surface_id;
  const gfx::Size viewport_size_for_tile_priority;
  const gfx::Transform transform_for_tile_priority;
  const bool offscreen_pre_raster;
  const float device_scale_factor;

  CopyOutputRequestQueue copy_requests;

  // Used for metrics, indicates that we invalidated for this frame.
  const bool did_invalidate;

  // Latest BeginFrameArgs this frame is being presented for so far. Normally
  // this corresponds to the begin frame of the current draw cycle (BeginFrame
  // => DrawOnUI => Sync => DrawOnRT), but in cases when DrawOnRT doesn't happen
  // (e.g webview is offscreen) this will be updated to more recent draws.
  // See: `HardwareRenderer::WaitAndPruneFrameQueue()` for details.
  viz::BeginFrameArgs begin_frame_args;

  // Indicates if this ChildFrame was rendered at least once.
  bool rendered = false;

  // Thread ids for passing to the HWUI ADPF session.
  base::flat_set<base::PlatformThreadId> renderer_thread_ids;
  base::PlatformThreadId browser_io_thread_id;
};

using ChildFrameQueue = base::circular_deque<std::unique_ptr<ChildFrame>>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_
