// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace viz {
class CompositorFrameSinkSupport;
class FrameSinkManagerImpl;
class ExternalBeginFrameSource;
}  // namespace viz

namespace android_webview {
class ChildFrame;

class RootFrameSinkClient {
 public:
  virtual ~RootFrameSinkClient() = default;

  virtual void SetNeedsBeginFrames(bool needs_begin_frame) = 0;
  virtual void Invalidate() = 0;
  virtual void ReturnResources(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      std::vector<viz::ReturnedResource> resources) = 0;
  virtual void OnCompositorFrameTransitionDirectiveProcessed(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) = 0;
};

// This class holds per-AwContents classes on the viz thread that do not need
// access to the GPU. It is single-threaded and refcounted on the viz thread.
// This needs to be separate from classes for rendering which requires GPU
// to enable sending begin frames independently from access to GPU.
//
// Lifetime: WebView
class RootFrameSink : public base::RefCounted<RootFrameSink>,
                      public viz::mojom::CompositorFrameSinkClient,
                      public viz::ExternalBeginFrameSourceClient {
 public:
  using SetNeedsBeginFrameCallback = base::RepeatingCallback<void(bool)>;
  RootFrameSink(RootFrameSinkClient* client);

  RootFrameSink(const RootFrameSink&) = delete;
  RootFrameSink& operator=(const RootFrameSink&) = delete;

  const viz::FrameSinkId& root_frame_sink_id() const {
    return root_frame_sink_id_;
  }

  const viz::LocalSurfaceId& SubmitRootCompositorFrame(
      viz::CompositorFrame frame);
  void EvictRootSurface(const viz::LocalSurfaceId& local_surface_id);

  void AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  bool BeginFrame(const viz::BeginFrameArgs& args, bool had_input_event);
  void SetBeginFrameSourcePaused(bool paused);
  void SetNeedsDraw(bool needs_draw);
  void OnNewUncommittedFrame(const viz::SurfaceId& surface_id);
  bool IsChildSurface(const viz::FrameSinkId& frame_sink_id);
  void DettachClient();
  void EvictChildSurface(const viz::SurfaceId& surface_id);
  void SetContainedSurfaces(const base::flat_set<viz::SurfaceId>& ids);
  void InvalidateForOverlays();

  void SubmitChildCompositorFrame(ChildFrame* child_frame);
  viz::FrameTimingDetailsMap TakeChildFrameTimingDetailsMap();
  gfx::Size GetChildFrameSize();
  base::flat_set<base::PlatformThreadId> GetChildFrameRendererThreadIds();

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& feedbacks,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}

  // viz::ExternalBeginFrameSourceClient overrides.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void OnCaptureStarted(const viz::FrameSinkId& frame_sink_id);

 private:
  friend class base::RefCounted<RootFrameSink>;
  class ChildCompositorFrameSink;

  ~RootFrameSink() override;
  viz::FrameSinkManagerImpl* GetFrameSinkManager();
  void ReturnResources(viz::FrameSinkId frame_sink_id,
                       uint32_t layer_tree_frame_sink_id,
                       std::vector<viz::ReturnedResource> resources);
  void OnCompositorFrameTransitionDirectiveProcessed(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id);

  bool HasPendingDependency(const viz::SurfaceId& surface_id);
  void UpdateNeedsBeginFrames(bool needs_begin_frame);
  bool ProcessVisibleSurfacesInvalidation();

  const viz::FrameSinkId root_frame_sink_id_;
  base::flat_set<viz::FrameSinkId> child_frame_sink_ids_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  viz::ParentLocalSurfaceIdAllocator root_local_surface_id_allocator_;
  gfx::Size root_surface_size_;
  float root_device_scale_factor_ = 0.0f;
  viz::FrameTokenGenerator next_root_frame_token_;

  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;

  std::unique_ptr<ChildCompositorFrameSink> child_sink_support_;
  base::flat_set<base::PlatformThreadId> child_frame_renderer_thread_ids_;

  bool clients_need_begin_frames_ = false;
  bool needs_begin_frames_ = false;

  bool needs_draw_ = false;
  raw_ptr<RootFrameSinkClient> client_;
  base::flat_set<viz::SurfaceId> contained_surfaces_;
  std::map<viz::SurfaceId, uint64_t> last_invalidated_frame_index_;

  const bool use_new_invalidate_heuristic_;

  THREAD_CHECKER(thread_checker_);
};

using RootFrameSinkGetter =
    base::RepeatingCallback<scoped_refptr<RootFrameSink>()>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_
