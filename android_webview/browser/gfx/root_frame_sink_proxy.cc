// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/root_frame_sink_proxy.h"

#include <utility>

#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace android_webview {

class RootFrameSinkProxy::RootFrameSinkClientImpl : public RootFrameSinkClient {
 public:
  RootFrameSinkClientImpl(RootFrameSinkProxy* owner) : owner_(owner) {}
  ~RootFrameSinkClientImpl() override = default;

  // RootFrameSinkClient implementation
  void SetNeedsBeginFrames(bool needs_begin_frame) override {
    owner_->SetNeedsBeginFramesOnViz(needs_begin_frame);
  }
  void Invalidate() override { owner_->InvalidateOnViz(); }
  void ReturnResources(viz::FrameSinkId frame_sink_id,
                       uint32_t layer_tree_frame_sink_id,
                       std::vector<viz::ReturnedResource> resources) override {
    owner_->ReturnResourcesOnViz(frame_sink_id, layer_tree_frame_sink_id,
                                 std::move(resources));
  }
  void OnCompositorFrameTransitionDirectiveProcessed(
      viz::FrameSinkId frame_sink_id,
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) override {
    owner_->OnCompositorFrameTransitionDirectiveProcessedOnViz(
        frame_sink_id, layer_tree_frame_sink_id, sequence_id);
  }

 private:
  const raw_ptr<RootFrameSinkProxy> owner_;
};

// static
scoped_refptr<RootFrameSink> RootFrameSinkProxy::GetRootFrameSinkHelper(
    base::WeakPtr<RootFrameSinkProxy> proxy) {
  DCHECK(VizCompositorThreadRunnerWebView::GetInstance()
             ->task_runner()
             ->BelongsToCurrentThread());
  if (proxy)
    return proxy->without_gpu_;
  return nullptr;
}

RootFrameSinkProxy::RootFrameSinkProxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    RootFrameSinkProxyClient* client,
    viz::BeginFrameSource* begin_frame_source)
    : ui_task_runner_(ui_task_runner),
      viz_task_runner_(
          VizCompositorThreadRunnerWebView::GetInstance()->task_runner()),
      client_(client),
      begin_frame_source_(begin_frame_source) {
  DETACH_FROM_THREAD(viz_thread_checker_);
  viz_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::InitializeOnViz,
                                base::Unretained(this)));
}

void RootFrameSinkProxy::InitializeOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  root_frame_sink_client_ = std::make_unique<RootFrameSinkClientImpl>(this);
  without_gpu_ =
      base::MakeRefCounted<RootFrameSink>(root_frame_sink_client_.get());
}

RootFrameSinkProxy::~RootFrameSinkProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::DestroyOnViz,
                                base::Unretained(this)));
  if (observing_bfs_)
    begin_frame_source_->RemoveObserver(this);
}

void RootFrameSinkProxy::DestroyOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  without_gpu_->DettachClient();
  without_gpu_.reset();
  weak_ptr_factory_on_viz_.InvalidateWeakPtrs();
  root_frame_sink_client_.reset();
}

void RootFrameSinkProxy::SetNeedsBeginFramesOnViz(bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::SetNeedsBeginFramesOnUI,
                     weak_ptr_factory_.GetWeakPtr(), needs_begin_frames));
}

void RootFrameSinkProxy::SetNeedsBeginFramesOnUI(bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  if (observing_bfs_ == needs_begin_frames)
    return;

  observing_bfs_ = needs_begin_frames;

  if (needs_begin_frames)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

void RootFrameSinkProxy::InvalidateOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&RootFrameSinkProxy::InvalidateOnUI,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void RootFrameSinkProxy::InvalidateOnUI() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  client_->Invalidate();
}

void RootFrameSinkProxy::AddChildFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_NE(frame_sink_id.client_id(), 0u);
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::AddChildFrameSinkIdOnViz,
                     weak_ptr_factory_on_viz_.GetWeakPtr(), frame_sink_id));
}

void RootFrameSinkProxy::AddChildFrameSinkIdOnViz(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  without_gpu_->AddChildFrameSinkId(frame_sink_id);
}

void RootFrameSinkProxy::RemoveChildFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::RemoveChildFrameSinkIdOnViz,
                     weak_ptr_factory_on_viz_.GetWeakPtr(), frame_sink_id));
}

void RootFrameSinkProxy::RemoveChildFrameSinkIdOnViz(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  without_gpu_->RemoveChildFrameSinkId(frame_sink_id);
}

void RootFrameSinkProxy::OnInputEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  had_input_event_ = true;
}

bool RootFrameSinkProxy::BeginFrame(const viz::BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  bool invalidate = false;
  VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::BeginFrameOnViz,
                                base::Unretained(this), args, had_input_event_,
                                &invalidate));
  had_input_event_ = false;
  return invalidate;
}

void RootFrameSinkProxy::BeginFrameOnViz(const viz::BeginFrameArgs& args,
                                         bool had_input_event,
                                         bool* invalidate) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  *invalidate = without_gpu_->BeginFrame(args, had_input_event);
}

void RootFrameSinkProxy::SetBeginFrameSourcePausedOnViz(bool paused) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  without_gpu_->SetBeginFrameSourcePaused(paused);
}

RootFrameSinkGetter RootFrameSinkProxy::GetRootFrameSinkCallback() {
  return base::BindRepeating(&RootFrameSinkProxy::GetRootFrameSinkHelper,
                             weak_ptr_factory_on_viz_.GetWeakPtr());
}

void RootFrameSinkProxy::OnBeginFrameSourcePausedChanged(bool paused) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::SetBeginFrameSourcePausedOnViz,
                     weak_ptr_factory_on_viz_.GetWeakPtr(), paused));
}

bool RootFrameSinkProxy::OnBeginFrameDerivedImpl(
    const viz::BeginFrameArgs& args) {
  DCHECK(client_);
  if (BeginFrame(args))
    client_->Invalidate();

  return true;
}

void RootFrameSinkProxy::ReturnResourcesOnUI(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  client_->ReturnResourcesFromViz(frame_sink_id, layer_tree_frame_sink_id,
                                  std::move(resources));
}
void RootFrameSinkProxy::ReturnResourcesOnViz(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::ReturnResourcesOnUI,
                     weak_ptr_factory_.GetWeakPtr(), frame_sink_id,
                     layer_tree_frame_sink_id, std::move(resources)));
}

void RootFrameSinkProxy::OnCompositorFrameTransitionDirectiveProcessedOnUI(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  client_->OnCompositorFrameTransitionDirectiveProcessed(
      frame_sink_id, layer_tree_frame_sink_id, sequence_id);
}

void RootFrameSinkProxy::OnCompositorFrameTransitionDirectiveProcessedOnViz(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RootFrameSinkProxy::
                         OnCompositorFrameTransitionDirectiveProcessedOnUI,
                     weak_ptr_factory_.GetWeakPtr(), frame_sink_id,
                     layer_tree_frame_sink_id, sequence_id));
}

}  // namespace android_webview
