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

// This class holds per-AwContents classes on the viz thread that do not need
// access to the GPU. It is single-threaded and refcounted on the viz thread.
// This needs to be separate from classes for rendering which requires GPU
// to enable sending begin frames independently from access to GPU.
class RootFrameSink : public base::RefCounted<RootFrameSink>,
                      public viz::mojom::CompositorFrameSinkClient,
                      public viz::ExternalBeginFrameSourceClient {
 public:
  using SetNeedsBeginFrameCallback = base::RepeatingCallback<void(bool)>;
  explicit RootFrameSink(SetNeedsBeginFrameCallback set_needs_begin_frame);

  viz::CompositorFrameSinkSupport* support() const { return support_.get(); }
  const viz::FrameSinkId& root_frame_sink_id() const {
    return root_frame_sink_id_;
  }
  void AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  bool BeginFrame(const viz::BeginFrameArgs& args);

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
  ~RootFrameSink() override;
  viz::FrameSinkManagerImpl* GetFrameSinkManager();

  const viz::FrameSinkId root_frame_sink_id_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;

  bool needs_begin_frames_ = false;
  SetNeedsBeginFrameCallback set_needs_begin_frame_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(RootFrameSink);
};

using RootFrameSinkGetter =
    base::RepeatingCallback<scoped_refptr<RootFrameSink>()>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_H_
