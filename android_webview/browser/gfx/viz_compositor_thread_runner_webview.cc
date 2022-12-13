// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"

#include <utility>

#include "android_webview/browser/gfx/task_queue_webview.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"

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
  base::Thread::Options options(base::ThreadType::kCompositing);
  CHECK(viz_thread_.StartWithOptions(std::move(options)));
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

  // Android doesn't support software compositing, but in some cases
  // unaccelerated canvas can use SharedBitmaps as resource so we create
  // SharedBitmapManager anyway.
  // TODO(1056184): Stop using SharedBitmapManager after fixing fallback to
  // SharedBitmap.
  server_shared_bitmap_manager_ =
      std::make_unique<viz::ServerSharedBitmapManager>();

  auto init_params = viz::FrameSinkManagerImpl::InitParams(
      server_shared_bitmap_manager_.get());

  if (base::FeatureList::IsEnabled(features::kWebViewNewInvalidateHeuristic)) {
    // HWUI has 2 frames pipelineing and we need another one because we force
    // client to be frame behind.
    init_params.max_uncommitted_frames = 3;
  }

  frame_sink_manager_ =
      std::make_unique<viz::FrameSinkManagerImpl>(init_params);
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

bool VizCompositorThreadRunnerWebView::CreateHintSessionFactory(
    base::flat_set<base::PlatformThreadId> thread_ids,
    base::RepeatingClosure* wake_up_closure) {
  return false;
}

void VizCompositorThreadRunnerWebView::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerParamsPtr params,
    viz::GpuServiceImpl* gpu_service) {
  // Does not support software compositing.
  DCHECK(gpu_service);

  viz_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunnerWebView::BindFrameSinkManagerOnViz,
          base::Unretained(this), std::move(params),
          base::Unretained(gpu_service)));
}

void VizCompositorThreadRunnerWebView::BindFrameSinkManagerOnViz(
    viz::mojom::FrameSinkManagerParamsPtr params,
    viz::GpuServiceImpl* gpu_service) {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  DCHECK(frame_sink_manager_);
  DCHECK(!gpu_service_impl_ || gpu_service_impl_ == gpu_service);
  gpu_service_impl_ = gpu_service;

  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), viz_task_runner_,
      std::move(params->frame_sink_manager_client));
}

viz::GpuServiceImpl* VizCompositorThreadRunnerWebView::GetGpuService() {
  DCHECK_CALLED_ON_VALID_THREAD(viz_thread_checker_);
  return gpu_service_impl_;
}

}  // namespace android_webview
