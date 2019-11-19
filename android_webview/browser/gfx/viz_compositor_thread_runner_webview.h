// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_VIZ_COMPOSITOR_THREAD_RUNNER_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_VIZ_COMPOSITOR_THREAD_RUNNER_WEBVIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "components/ui_devtools/buildflags.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"

namespace base {
class Location;
}  // namespace base

namespace viz {
class FrameSinkManagerImpl;
}  // namespace viz

namespace android_webview {

// This class overrides VizCompositorThreadRunner largely so that
// FrameSinkManagerImpl and other viz classes are connected to their mojo end
// points using the same code path (through VizMainImpl) as other chromium
// platforms. The benefit of this include:
// * code path sharing
// * keep illusion in content and below that webview does not assume
//   in-process gpu
// * no need to introduce more "if webview" conditions in shared code
// However these viz classes actually do not talk to the gpu service
// in VizMainImpl, which may cause confusion for developers. If this proves to
// be common, then an alternative is assume viz runs in the browser process
// and directly connect viz classes to mojo end points in the browser.
class VizCompositorThreadRunnerWebView : public viz::VizCompositorThreadRunner {
 public:
  static VizCompositorThreadRunnerWebView* GetInstance();

  viz::FrameSinkManagerImpl* GetFrameSinkManager();

  // Must be called from the TaskQueueWebView thread. |task| is allowed to call
  // TaskQueueWebView::ScheduleTask.
  void ScheduleOnVizAndBlock(base::OnceClosure task);

  // Can be called from any thread, and blocks the caller until the task
  // finishes executing.
  void PostTaskAndBlock(const base::Location& from_here,
                        base::OnceClosure task);

  // viz::VizCompositorThreadRunner overrides.
  base::SingleThreadTaskRunner* task_runner() override;
  void CreateFrameSinkManager(
      viz::mojom::FrameSinkManagerParamsPtr params) override;
  void CreateFrameSinkManager(viz::mojom::FrameSinkManagerParamsPtr params,
                              gpu::CommandBufferTaskExecutor* task_executor,
                              viz::GpuServiceImpl* gpu_service) override;
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  void CreateVizDevTools(viz::mojom::VizDevToolsParamsPtr params) override;
#endif
  void CleanupForShutdown(base::OnceClosure cleanup_finished_callback) override;

 private:
  friend class base::NoDestructor<VizCompositorThreadRunnerWebView>;

  VizCompositorThreadRunnerWebView();
  ~VizCompositorThreadRunnerWebView() override;

  void InitFrameSinkManagerOnViz();
  void BindFrameSinkManagerOnViz(viz::mojom::FrameSinkManagerParamsPtr params);

  base::Thread viz_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;

  // Only accessed on |viz_task_runner_|.
  THREAD_CHECKER(viz_thread_checker_);
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(VizCompositorThreadRunnerWebView);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_VIZ_COMPOSITOR_THREAD_RUNNER_WEBVIEW_H_
