// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/root_frame_sink_proxy.h"

#include <utility>

#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"

namespace android_webview {

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
    SetNeedsBeginFrameCallback set_needs_begin_frame_callback)
    : ui_task_runner_(ui_task_runner),
      viz_task_runner_(
          VizCompositorThreadRunnerWebView::GetInstance()->task_runner()),
      set_needs_begin_frame_callback_(
          std::move(set_needs_begin_frame_callback)) {
  DETACH_FROM_THREAD(viz_thread_checker_);
  viz_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::InitializeOnViz,
                                base::Unretained(this)));
}

void RootFrameSinkProxy::InitializeOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  without_gpu_ = base::MakeRefCounted<RootFrameSink>(
      base::BindRepeating(&RootFrameSinkProxy::SetNeedsBeginFramesOnViz,
                          weak_ptr_factory_on_viz_.GetWeakPtr()));
}

RootFrameSinkProxy::~RootFrameSinkProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::DestroyOnViz,
                                base::Unretained(this)));
}

void RootFrameSinkProxy::DestroyOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(without_gpu_->HasOneRef());
  without_gpu_.reset();
  weak_ptr_factory_on_viz_.InvalidateWeakPtrs();
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
  set_needs_begin_frame_callback_.Run(needs_begin_frames);
}

void RootFrameSinkProxy::AddChildFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
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

bool RootFrameSinkProxy::BeginFrame(const viz::BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  bool invalidate = false;
  VizCompositorThreadRunnerWebView::GetInstance()->PostTaskAndBlock(
      FROM_HERE, base::BindOnce(&RootFrameSinkProxy::BeginFrameOnViz,
                                base::Unretained(this), args, &invalidate));
  return invalidate;
}

void RootFrameSinkProxy::BeginFrameOnViz(const viz::BeginFrameArgs& args,
                                         bool* invalidate) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  *invalidate = without_gpu_->BeginFrame(args);
}

RootFrameSinkGetter RootFrameSinkProxy::GetRootFrameSinkCallback() {
  return base::BindRepeating(&RootFrameSinkProxy::GetRootFrameSinkHelper,
                             weak_ptr_factory_on_viz_.GetWeakPtr());
}

}  // namespace android_webview
