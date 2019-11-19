// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"

#include <utility>

#include "android_webview/browser/gfx/task_queue_web_view.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace android_webview {

namespace {

void RunAndDone(base::OnceClosure viz_task, base::OnceClosure done_task) {
  std::move(viz_task).Run();

  // |done_task| is provided by TaskQueueWebView unblocks the gpu service.
  std::move(done_task).Run();
}

void RunAndSignal(base::OnceClosure viz_task, base::WaitableEvent* done) {
  std::move(viz_task).Run();
  done->Signal();
}

}  // namespace

// static
VizCompositorThreadRunnerWebView*
VizCompositorThreadRunnerWebView::GetInstance() {
  static base::NoDestructor<VizCompositorThreadRunnerWebView> instance;
  return instance.get();
}

VizCompositorThreadRunnerWebView::VizCompositorThreadRunnerWebView()
    : viz_thread_("VizWebView") {
  base::Thread::Options options;
  options.priority = base::ThreadPriority::DISPLAY;
  CHECK(viz_thread_.StartWithOptions(options));
  viz_task_runner_ = viz_thread_.task_runner();
  TaskQueueWebView::GetInstance()->InitializeVizThread(viz_task_runner_);

  DETACH_FROM_THREAD(viz_thread_checker_);

  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunnerWebView::InitFrameSinkManagerOnViz,
          base::Unretained(this)));
}

void VizCompositorThreadRunnerWebView::InitFrameSinkManagerOnViz() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);

  // The SharedBitmapManager is null as we do not support or use software
  // compositing on Android.
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      /*shared_bitmap_manager=*/nullptr);
}

viz::FrameSinkManagerImpl*
VizCompositorThreadRunnerWebView::GetFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(frame_sink_manager_);
  return frame_sink_manager_.get();
}

void VizCompositorThreadRunnerWebView::ScheduleOnVizAndBlock(
    base::OnceClosure task) {
  TaskQueueWebView::GetInstance()->ScheduleOnVizAndBlock(
      base::BindOnce(&RunAndDone, std::move(task)));
}

void VizCompositorThreadRunnerWebView::PostTaskAndBlock(
    const base::Location& from_here,
    base::OnceClosure task) {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent e;
  task_runner()->PostTask(from_here,
                          base::BindOnce(&RunAndSignal, std::move(task), &e));
  e.Wait();
}

VizCompositorThreadRunnerWebView::~VizCompositorThreadRunnerWebView() = default;

base::SingleThreadTaskRunner* VizCompositorThreadRunnerWebView::task_runner() {
  return viz_task_runner_.get();
}

void VizCompositorThreadRunnerWebView::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerParamsPtr params) {
  // Does not support software compositing.
  NOTREACHED();
}

void VizCompositorThreadRunnerWebView::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    viz::GpuServiceImpl* gpu_service) {
  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunnerWebView::BindFrameSinkManagerOnViz,
          base::Unretained(this), std::move(params)));
}

void VizCompositorThreadRunnerWebView::BindFrameSinkManagerOnViz(
    viz::mojom::FrameSinkManagerParamsPtr params) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(frame_sink_manager_);

  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), viz_task_runner_,
      std::move(params->frame_sink_manager_client));
}

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
void VizCompositorThreadRunnerWebView::CreateVizDevTools(
    viz::mojom::VizDevToolsParamsPtr params) {
  NOTIMPLEMENTED();
}
#endif

void VizCompositorThreadRunnerWebView::CleanupForShutdown(
    base::OnceClosure cleanup_finished_callback) {
  // In-process gpu is not supposed to shutdown.
  // Plus viz thread in webview architecture is not owned by the gpu thread.
  NOTREACHED();
}

}  // namespace android_webview
