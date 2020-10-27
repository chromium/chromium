// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_

#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"

namespace android_webview {

class TaskQueueWebView;
class GpuServiceWebView;

// Implementation for access to gpu objects and task queue for WebView.
class SkiaOutputSurfaceDependencyWebView
    : public viz::SkiaOutputSurfaceDependency {
 public:
  SkiaOutputSurfaceDependencyWebView(
      TaskQueueWebView* task_queue,
      GpuServiceWebView* gpu_service,
      gpu::SharedContextState* shared_context_state,
      gl::GLSurface* gl_surface);
  ~SkiaOutputSurfaceDependencyWebView() override;

  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  const gpu::GpuDriverBugWorkarounds& GetGpuDriverBugWorkarounds() override;
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  gpu::raster::GrShaderCache* GetGrShaderCache() override;
  viz::VulkanContextProvider* GetVulkanContextProvider() override;
  viz::DawnContextProvider* GetDawnContextProvider() override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() override;
  gpu::MailboxManager* GetMailboxManager() override;
  gpu::ImageFactory* GetGpuImageFactory() override;
  void ScheduleGrContextCleanup() override;
  void ScheduleDelayedGPUTaskFromGPUThread(base::OnceClosure task) override;
  void PostTaskToClientThread(base::OnceClosure closure) override;
  bool IsOffscreen() override;
  gpu::SurfaceHandle GetSurfaceHandle() override;
  scoped_refptr<gl::GLSurface> CreateGLSurface(
      base::WeakPtr<gpu::ImageTransportSurfaceDelegate> stub,
      gl::GLSurfaceFormat format) override;
  base::ScopedClosureRunner CacheGLSurface(gl::GLSurface* surface) override;
  void RegisterDisplayContext(gpu::DisplayContext* display_context) override;
  void UnregisterDisplayContext(gpu::DisplayContext* display_context) override;
  void DidLoseContext(gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;

  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;
  bool NeedsSupportForExternalStencil() override;

 private:
  gl::GLSurface* const gl_surface_;
  TaskQueueWebView* task_queue_;
  GpuServiceWebView* gpu_service_;
  gpu::GpuDriverBugWorkarounds workarounds_;
  gpu::SharedContextState* const shared_context_state_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceDependencyWebView);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SKIA_OUTPUT_SURFACE_DEPENDENCY_WEBVIEW_H_
