// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
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
};

// This class holds per-AwContents classes on the viz thread that do not need
// access to the GPU. It is single-threaded and refcounted on the viz thread.
// This needs to be separate from classes for rendering which requires GPU
// to enable sending begin frames independently from access to GPU.
class RootFrameSink : public base::RefCounted<RootFrameSink>,
                      public viz::mojom::CompositorFrameSinkClient,
                      public viz::ExternalBeginFrameSourceClient {
 public:
  using SetNeedsBeginFrameCallback = base::RepeatingCallback<void(bool)>;
  RootFrameSink(RootFrameSinkClient* client);

  viz::CompositorFrameSinkSupport* support() const { return support_.get(); }
  const viz::FrameSinkId& root_frame_sink_id() const {
    return root_frame_sink_id_;
  }
  void AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  bool BeginFrame(const viz::BeginFrameArgs& args, bool had_input_event);
  void SetBeginFrameSourcePaused(bool paused);
  void SetNeedsDraw(bool needs_draw);
  bool IsChildSurface(const viz::FrameSinkId& frame_sink_id);
  void DettachClient();

  void SubmitChildCompositorFrame(ChildFrame* child_frame);
  viz::FrameTimingDetailsMap TakeChildFrameTimingDetailsMap();
  gfx::Size GetChildFrameSize();

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& feedbacks) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;

  // viz::ExternalBeginFrameSourceClient overrides.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

 private:
  friend class base::RefCounted<RootFrameSink>;
  class ChildCompositorFrameSink;

  ~RootFrameSink() override;
  viz::FrameSinkManagerImpl* GetFrameSinkManager();
  void ReturnResources(viz::FrameSinkId frame_sink_id,
                       uint32_t layer_tree_frame_sink_id,
                       std::vector<viz::ReturnedResource> resources);

  const viz::FrameSinkId root_frame_sink_id_;
  base::flat_set<viz::FrameSinkId> child_frame_sink_ids_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;

  std::unique_ptr<ChildCompositorFrameSink> child_sink_support_;

  bool needs_begin_frames_ = false;
  bool needs_draw_ = false;
  RootFrameSinkClient* client_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(RootFrameSink);
};

using RootFrameSinkGetter =
    base::RepeatingCallback<scoped_refptr<RootFrameSink>()>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_
