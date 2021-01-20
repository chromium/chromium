// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_PROXY_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_PROXY_H_

#include "android_webview/browser/gfx/root_frame_sink.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace viz {
struct BeginFrameArgs;
}

namespace android_webview {

class RootFrameSinkProxyClient {
 public:
  virtual void Invalidate() = 0;
  virtual void ReturnResourcesFromViz(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      std::vector<viz::ReturnedResource> resources) = 0;
};

// Per-AwContents object. Straddles UI and Viz thread. Public methods should be
// called on the UI thread unless otherwise specified. Mostly used for creating
// RootFrameSink and routing calls to it.
class RootFrameSinkProxy : public viz::BeginFrameObserverBase {
 public:
  RootFrameSinkProxy(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      RootFrameSinkProxyClient* client,
      viz::BeginFrameSource* begin_frame_source);
  ~RootFrameSinkProxy() override;

  void AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void OnInputEvent();

  // The returned callback can only be called on viz thread.
  RootFrameSinkGetter GetRootFrameSinkCallback();

 private:
  class RootFrameSinkClientImpl;
  static scoped_refptr<RootFrameSink> GetRootFrameSinkHelper(
      base::WeakPtr<RootFrameSinkProxy> proxy);

  void InitializeOnViz();
  void DestroyOnViz();
  void AddChildFrameSinkIdOnViz(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkIdOnViz(const viz::FrameSinkId& frame_sink_id);
  void BeginFrameOnViz(const viz::BeginFrameArgs& args,
                       bool had_input_event,
                       bool* invalidate);
  void SetNeedsBeginFramesOnViz(bool needs_begin_frames);
  void SetNeedsBeginFramesOnUI(bool needs_begin_frames);
  void SetBeginFrameSourcePausedOnViz(bool paused);
  void InvalidateOnViz();
  void InvalidateOnUI();
  void ReturnResourcesOnViz(viz::FrameSinkId frame_sink_id,
                            uint32_t layer_tree_frame_sink_id,
                            std::vector<viz::ReturnedResource> resources);
  void ReturnResourcesOnUI(viz::FrameSinkId frame_sink_id,
                           uint32_t layer_tree_frame_sink_id,
                           std::vector<viz::ReturnedResource> resources);

  bool BeginFrame(const viz::BeginFrameArgs& args);

  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;
  RootFrameSinkProxyClient* const client_;
  std::unique_ptr<RootFrameSinkClient> root_frame_sink_client_;
  scoped_refptr<RootFrameSink> without_gpu_;
  viz::BeginFrameSource* const begin_frame_source_;
  bool had_input_event_ = false;
  bool observing_bfs_ = false;

  THREAD_CHECKER(ui_thread_checker_);
  THREAD_CHECKER(viz_thread_checker_);

  base::WeakPtrFactory<RootFrameSinkProxy> weak_ptr_factory_{this};
  base::WeakPtrFactory<RootFrameSinkProxy> weak_ptr_factory_on_viz_{this};

  DISALLOW_COPY_AND_ASSIGN(RootFrameSinkProxy);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_ROOT_FRAME_SINK_PROXY_H_
