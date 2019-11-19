// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"

#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "android_webview/browser/gfx/parent_output_surface.h"
#include "android_webview/browser/gfx/task_forwarding_sequence.h"
#include "android_webview/browser/gfx/task_queue_web_view.h"

namespace android_webview {

SkiaOutputSurfaceDependencyWebView::SkiaOutputSurfaceDependencyWebView(
    TaskQueueWebView* task_queue,
    GpuServiceWebView* gpu_service,
    gpu::SharedContextState* shared_context_state,
    gl::GLSurface* gl_surface)
    : gl_surface_(gl_surface),
      task_queue_(task_queue),
      gpu_service_(gpu_service),
      workarounds_(
          gpu_service_->gpu_feature_info().enabled_gpu_driver_bug_workarounds),
      shared_context_state_(shared_context_state) {}

SkiaOutputSurfaceDependencyWebView::~SkiaOutputSurfaceDependencyWebView() =
    default;

std::unique_ptr<gpu::SingleTaskSequence>
SkiaOutputSurfaceDependencyWebView::CreateSequence() {
  return std::make_unique<TaskForwardingSequence>(
      this->task_queue_, this->gpu_service_->sync_point_manager());
}

bool SkiaOutputSurfaceDependencyWebView::IsUsingVulkan() {
  return shared_context_state_ && shared_context_state_->GrContextIsVulkan();
}

bool SkiaOutputSurfaceDependencyWebView::IsUsingDawn() {
  return false;
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
  return shared_context_state_;
}

gpu::raster::GrShaderCache*
SkiaOutputSurfaceDependencyWebView::GetGrShaderCache() {
  return nullptr;
}

viz::VulkanContextProvider*
SkiaOutputSurfaceDependencyWebView::GetVulkanContextProvider() {
  return shared_context_state_->vk_context_provider();
}

viz::DawnContextProvider*
SkiaOutputSurfaceDependencyWebView::GetDawnContextProvider() {
  return nullptr;
}

const gpu::GpuPreferences&
SkiaOutputSurfaceDependencyWebView::GetGpuPreferences() {
  return gpu_service_->gpu_preferences();
}

const gpu::GpuFeatureInfo&
SkiaOutputSurfaceDependencyWebView::GetGpuFeatureInfo() {
  return gpu_service_->gpu_feature_info();
}

gpu::MailboxManager* SkiaOutputSurfaceDependencyWebView::GetMailboxManager() {
  return gpu_service_->mailbox_manager();
}

void SkiaOutputSurfaceDependencyWebView::ScheduleGrContextCleanup() {
  // There is no way to access the gpu thread here, so leave it no-op for now.
}

void SkiaOutputSurfaceDependencyWebView::PostTaskToClientThread(
    base::OnceClosure closure) {
  task_queue_->ScheduleClientTask(std::move(closure));
}

gpu::ImageFactory* SkiaOutputSurfaceDependencyWebView::GetGpuImageFactory() {
  return nullptr;
}

bool SkiaOutputSurfaceDependencyWebView::IsOffscreen() {
  return false;
}

gpu::SurfaceHandle SkiaOutputSurfaceDependencyWebView::GetSurfaceHandle() {
  return gpu::kNullSurfaceHandle;
}

scoped_refptr<gl::GLSurface>
SkiaOutputSurfaceDependencyWebView::CreateGLSurface(
    base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub) {
  return gl_surface_;
}

void SkiaOutputSurfaceDependencyWebView::RegisterDisplayContext(
    gpu::DisplayContext* display_context) {
  // No GpuChannelManagerDelegate here, so leave it no-op for now.
}

void SkiaOutputSurfaceDependencyWebView::UnregisterDisplayContext(
    gpu::DisplayContext* display_context) {
  // No GpuChannelManagerDelegate here, so leave it no-op for now.
}

void SkiaOutputSurfaceDependencyWebView::DidLoseContext(
    bool offscreen,
    gpu::error::ContextLostReason reason,
    const GURL& active_url) {
  // No GpuChannelManagerDelegate here, so leave it no-op for now.
  LOG(ERROR) << "SkiaRenderer detected lost context.";
}

base::TimeDelta
SkiaOutputSurfaceDependencyWebView::GetGpuBlockedTimeSinceLastSwap() {
  // WebView doesn't track how long GPU thread was blocked
  return base::TimeDelta();
}

}  // namespace android_webview
