// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/task_forwarding_sequence.h"
#include "android_webview/browser/gfx/task_queue_webview.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "ui/gl/gl_surface.h"

namespace android_webview {

SkiaOutputSurfaceDependencyWebView::SkiaOutputSurfaceDependencyWebView(
    TaskQueueWebView* task_queue,
    GpuServiceWebView* gpu_service,
    gpu::SharedContextState* shared_context_state,
    gl::GLSurface* gl_surface,
    AwVulkanContextProvider* vulkan_context_provider)
    : gl_surface_(gl_surface),
      vulkan_context_provider_(vulkan_context_provider),
      task_queue_(task_queue),
      gpu_service_(gpu_service),
      workarounds_(
          gpu_service_->gpu_feature_info().enabled_gpu_driver_bug_workarounds),
      shared_context_state_(shared_context_state) {
  DCHECK(!(shared_context_state_ && vulkan_context_provider_) ||
         shared_context_state_->vk_context_provider() ==
             vulkan_context_provider);
}

SkiaOutputSurfaceDependencyWebView::~SkiaOutputSurfaceDependencyWebView() =
    default;

std::unique_ptr<gpu::SingleTaskSequence>
SkiaOutputSurfaceDependencyWebView::CreateSequence() {
  return std::make_unique<TaskForwardingSequence>(
      task_queue_, gpu_service_->sync_point_manager(),
      gpu_service_->scheduler());
}

gpu::SharedImageManager*
SkiaOutputSurfaceDependencyWebView::GetSharedImageManager() {
  return gpu_service_->shared_image_manager();
}

gpu::SyncPointManager*
SkiaOutputSurfaceDependencyWebView::GetSyncPointManager() {
  return gpu_service_->sync_point_manager();
}

const gpu::GpuDriverBugWorkarounds&
SkiaOutputSurfaceDependencyWebView::GetGpuDriverBugWorkarounds() {
  return workarounds_;
}

scoped_refptr<gpu::SharedContextState>
SkiaOutputSurfaceDependencyWebView::GetSharedContextState() {
  return shared_context_state_.get();
}

gpu::raster::GrShaderCache*
SkiaOutputSurfaceDependencyWebView::GetGrShaderCache() {
  return nullptr;
}

viz::VulkanContextProvider*
SkiaOutputSurfaceDependencyWebView::GetVulkanContextProvider() {
  return shared_context_state_->vk_context_provider();
}

gpu::DawnContextProvider*
SkiaOutputSurfaceDependencyWebView::GetDawnContextProvider() {
  return nullptr;
}

const gpu::GpuPreferences&
SkiaOutputSurfaceDependencyWebView::GetGpuPreferences() const {
  return gpu_service_->gpu_preferences();
}

const gpu::GpuFeatureInfo&
SkiaOutputSurfaceDependencyWebView::GetGpuFeatureInfo() {
  return gpu_service_->gpu_feature_info();
}

void SkiaOutputSurfaceDependencyWebView::ScheduleGrContextCleanup() {
  shared_context_state_->ScheduleSkiaCleanup();
}

scoped_refptr<base::SingleThreadTaskRunner>
SkiaOutputSurfaceDependencyWebView::GetClientTaskRunner() {
  return task_queue_->GetClientTaskRunner();
}

bool SkiaOutputSurfaceDependencyWebView::IsOffscreen() {
  return false;
}

gpu::SurfaceHandle SkiaOutputSurfaceDependencyWebView::GetSurfaceHandle() {
  return gpu::kNullSurfaceHandle;
}

scoped_refptr<gl::Presenter>
SkiaOutputSurfaceDependencyWebView::CreatePresenter() {
  return nullptr;
}
scoped_refptr<gl::GLSurface>
SkiaOutputSurfaceDependencyWebView::CreateGLSurface(
    gl::GLSurfaceFormat format) {
  return gl_surface_.get();
}

base::ScopedClosureRunner SkiaOutputSurfaceDependencyWebView::CachePresenter(
    gl::Presenter* presenter) {
  NOTREACHED();
}

base::ScopedClosureRunner SkiaOutputSurfaceDependencyWebView::CacheGLSurface(
    gl::GLSurface* surface) {
  NOTREACHED();
}

void SkiaOutputSurfaceDependencyWebView::DidLoseContext(
    gpu::error::ContextLostReason reason,
    const GURL& active_url) {
  // No GpuChannelManagerDelegate here, so leave it no-op for now.
  LOG(ERROR) << "SkiaRenderer detected lost context.";
}

void SkiaOutputSurfaceDependencyWebView::ScheduleDelayedGPUTaskFromGPUThread(
    base::OnceClosure task) {
  task_queue_->ScheduleIdleTask(std::move(task));
}

bool SkiaOutputSurfaceDependencyWebView::NeedsSupportForExternalStencil() {
  return true;
}

bool SkiaOutputSurfaceDependencyWebView::IsUsingCompositorGpuThread() {
  // Webview never uses CompositorGpuThread aka DrDc thread.
  return false;
}

}  // namespace android_webview
