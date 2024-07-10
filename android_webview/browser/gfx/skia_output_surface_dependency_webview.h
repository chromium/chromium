// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"

namespace android_webview {

class AwVulkanContextProvider;
class TaskQueueWebView;
class GpuServiceWebView;

// Implementation for access to gpu objects and task queue for WebView.
//
// Lifetime: WebView
class SkiaOutputSurfaceDependencyWebView
    : public viz::SkiaOutputSurfaceDependency {
 public:
  SkiaOutputSurfaceDependencyWebView(
      TaskQueueWebView* task_queue,
      GpuServiceWebView* gpu_service,
      gpu::SharedContextState* shared_context_state,
      gl::GLSurface* gl_surface,
      AwVulkanContextProvider* vulkan_context_provider);

  SkiaOutputSurfaceDependencyWebView(
      const SkiaOutputSurfaceDependencyWebView&) = delete;
  SkiaOutputSurfaceDependencyWebView& operator=(
      const SkiaOutputSurfaceDependencyWebView&) = delete;

  ~SkiaOutputSurfaceDependencyWebView() override;

  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  const gpu::GpuDriverBugWorkarounds& GetGpuDriverBugWorkarounds() override;
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  gpu::raster::GrShaderCache* GetGrShaderCache() override;
  viz::VulkanContextProvider* GetVulkanContextProvider() override;
  gpu::DawnContextProvider* GetDawnContextProvider() override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() override;
  void ScheduleGrContextCleanup() override;
  void ScheduleDelayedGPUTaskFromGPUThread(base::OnceClosure task) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner() override;
  bool IsOffscreen() override;
  gpu::SurfaceHandle GetSurfaceHandle() override;
  scoped_refptr<gl::Presenter> CreatePresenter() override;
  scoped_refptr<gl::GLSurface> CreateGLSurface(
      gl::GLSurfaceFormat format) override;
  base::ScopedClosureRunner CachePresenter(gl::Presenter* presenter) override;
  base::ScopedClosureRunner CacheGLSurface(gl::GLSurface* surface) override;
  void DidLoseContext(gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;

  bool NeedsSupportForExternalStencil() override;
  bool IsUsingCompositorGpuThread() override;

 private:
  const raw_ptr<gl::GLSurface> gl_surface_;
  raw_ptr<AwVulkanContextProvider> vulkan_context_provider_;
  raw_ptr<TaskQueueWebView> task_queue_;
  raw_ptr<GpuServiceWebView> gpu_service_;
  gpu::GpuDriverBugWorkarounds workarounds_;
  const raw_ptr<gpu::SharedContextState> shared_context_state_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_
