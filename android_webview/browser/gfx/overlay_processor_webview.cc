// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/overlay_processor_webview.h"

#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "gpu/ipc/single_task_sequence.h"
#include "ui/gfx/android/android_surface_control_compat.h"

namespace android_webview {
namespace {

constexpr gpu::CommandBufferNamespace kOverlayProcessorNamespace =
    gpu::CommandBufferNamespace::IN_PROCESS;

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id) {
  return GpuServiceWebView::GetInstance()
      ->sync_point_manager()
      ->CreateSyncPointClientState(kOverlayProcessorNamespace,
                                   command_buffer_id, sequence_id);
}

}  // namespace

class OverlayProcessorWebView::Manager
    : public base::RefCountedThreadSafe<OverlayProcessorWebView::Manager> {
 public:
  Manager(gpu::MemoryTracker* memory_tracker,
          gpu::CommandBufferId command_buffer_id,
          gpu::SequenceId sequence_id)
      : shared_image_manager_(
            GpuServiceWebView::GetInstance()->shared_image_manager()),
        memory_tracker_(
            std::make_unique<gpu::MemoryTypeTracker>(memory_tracker)),
        sync_point_client_state_(
            CreateSyncPointClientState(command_buffer_id, sequence_id)) {}

  void SetGpuService(viz::GpuServiceImpl* gpu_service) {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

    DCHECK_EQ(shared_image_manager_, gpu_service->shared_image_manager());
    gpu_task_runner_ = gpu_service->main_runner();
  }

  absl::optional<gfx::SurfaceControl::Transaction> TakeHWUITransaction() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);

    absl::optional<gfx::SurfaceControl::Transaction> result;
    if (hwui_transaction_) {
      result.swap(hwui_transaction_);
    }
    return result;
  }

 private:
  friend class OverlayProcessorWebView::ScopedSurfaceControlAvailable;
  friend class base::RefCountedThreadSafe<Manager>;

  ~Manager() {
    DCHECK(!hwui_transaction_);
    DCHECK(!parent_surface_);
  }

  gfx::SurfaceControl::Transaction& GetHWUITransaction() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    if (!hwui_transaction_)
      hwui_transaction_.emplace();
    return hwui_transaction_.value();
  }

  const gfx::SurfaceControl::Surface& GetParentSurface() {
    DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
    if (!parent_surface_) {
      DCHECK(get_surface_control_);
      parent_surface_ =
          gfx::SurfaceControl::Surface::WrapUnowned(get_surface_control_());
      DCHECK(parent_surface_);
    }
    return *parent_surface_;
  }

  gpu::SharedImageManager* const shared_image_manager_;
  std::unique_ptr<gpu::MemoryTypeTracker> memory_tracker_;

  // GPU Main Thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // SyncPointClientState for render thread sequence.
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;

  scoped_refptr<gfx::SurfaceControl::Surface> parent_surface_;
  absl::optional<gfx::SurfaceControl::Transaction> hwui_transaction_;

  GetSurfaceControlFn get_surface_control_ = nullptr;

  THREAD_CHECKER(render_thread_checker_);
};

OverlayProcessorWebView::OverlayProcessorWebView(
    viz::DisplayCompositorMemoryAndTaskController* display_controller)
    : command_buffer_id_(gpu::DisplayCompositorMemoryAndTaskControllerOnGpu::
                             NextCommandBufferId()),
      render_thread_sequence_(display_controller->gpu_task_scheduler()) {
  base::WaitableEvent event;
  render_thread_sequence_->ScheduleGpuTask(
      base::BindOnce(
          &OverlayProcessorWebView::CreateManagerOnRT, base::Unretained(this),
          display_controller->controller_on_gpu(), command_buffer_id_,
          render_thread_sequence_->GetSequenceId(), &event),
      std::vector<gpu::SyncToken>());
  event.Wait();
}

OverlayProcessorWebView::~OverlayProcessorWebView() {
  render_thread_sequence_->ScheduleGpuTask(
      base::BindOnce(
          [](scoped_refptr<Manager> manager) {
            // manager leaves scope.
          },
          std::move(manager_)),
      std::vector<gpu::SyncToken>());
}

void OverlayProcessorWebView::CreateManagerOnRT(
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* controller_on_gpu,
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id,
    base::WaitableEvent* event) {
  manager_ = base::MakeRefCounted<Manager>(controller_on_gpu->memory_tracker(),
                                           command_buffer_id, sequence_id);
  event->Signal();
}

void OverlayProcessorWebView::SetOverlaysEnabledByHWUI(bool enabled) {
  overlays_enabled_by_hwui_ = enabled;
}

void OverlayProcessorWebView::RemoveOverlays() {
  overlays_enabled_by_hwui_ = false;
  // TODO(vasilyt): Remove overlays.
  NOTIMPLEMENTED();
}

absl::optional<gfx::SurfaceControl::Transaction>
OverlayProcessorWebView::TakeSurfaceTransactionOnRT() {
  DCHECK(manager_);
  return manager_->TakeHWUITransaction();
}

void OverlayProcessorWebView::CheckOverlaySupport(
    const viz::OverlayProcessorInterface::OutputSurfaceOverlayPlane*
        primary_plane,
    viz::OverlayCandidateList* candidates) {
  // If HWUI doesn't want us to overlay, we shouldn't.
  if (!overlays_enabled_by_hwui_)
    return;

  // We need GpuServiceImpl (one for Gpu Main Thread, not GpuServiceWebView) to
  // use overlays. It takes time to initialize it, so we don't block
  // RenderThread for it. Instead we're just polling here if it's done.
  if (!gpu_thread_sequence_) {
    viz::GpuServiceImpl* gpu_service =
        VizCompositorThreadRunnerWebView::GetInstance()->GetGpuService();
    if (!gpu_service)
      return;

    gpu_thread_sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service->GetGpuScheduler());

    render_thread_sequence_->ScheduleGpuTask(
        base::BindOnce(&OverlayProcessorWebView::Manager::SetGpuService,
                       base::Unretained(manager_.get()), gpu_service),
        std::vector<gpu::SyncToken>());
  }

  // Check candidates if they can be used with surface control.
  OverlayProcessorSurfaceControl::CheckOverlaySupport(primary_plane,
                                                      candidates);
}

void OverlayProcessorWebView::TakeOverlayCandidates(
    viz::OverlayCandidateList* candidate_list) {
  overlay_candidates_.swap(*candidate_list);
  candidate_list->clear();
}

void OverlayProcessorWebView::ScheduleOverlays(
    viz::DisplayResourceProvider* resource_provider) {
  DCHECK(!resource_provider_ || resource_provider_ == resource_provider_);
  resource_provider_ = resource_provider;

  DCHECK(overlay_candidates_.empty());
}

OverlayProcessorWebView::ScopedSurfaceControlAvailable::
    ScopedSurfaceControlAvailable(OverlayProcessorWebView* processor,
                                  GetSurfaceControlFn surface_getter)
    : processor_(processor) {
  DCHECK(processor_);
  DCHECK(processor_->manager_);
  processor_->manager_->get_surface_control_ = surface_getter;
}

OverlayProcessorWebView::ScopedSurfaceControlAvailable::
    ~ScopedSurfaceControlAvailable() {
  processor_->manager_->get_surface_control_ = nullptr;
}

}  // namespace android_webview
