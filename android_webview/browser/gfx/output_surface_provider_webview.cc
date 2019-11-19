// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/output_surface_provider_webview.h"

#include "android_webview/browser/gfx/aw_render_thread_context_provider.h"
#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/deferred_gpu_command_service.h"
#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "android_webview/browser/gfx/parent_output_surface.h"
#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"
#include "android_webview/browser/gfx/task_queue_web_view.h"
#include "android_webview/common/aw_switches.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {

void OnContextLost() {
  NOTREACHED() << "Non owned context lost!";
}

}  // namespace

OutputSurfaceProviderWebview::OutputSurfaceProviderWebview() {
  // Should be kept in sync with compositor_impl_android.cc.
  renderer_settings_.allow_antialiasing = false;
  renderer_settings_.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  renderer_settings_.should_clear_root_render_pass = false;

  renderer_settings_.use_skia_renderer = features::IsUsingSkiaRenderer();

  auto* command_line = base::CommandLine::ForCurrentProcess();
  enable_vulkan_ = command_line->HasSwitch(switches::kWebViewEnableVulkan);
  enable_shared_image_ =
      command_line->HasSwitch(switches::kWebViewEnableSharedImage);
  LOG_IF(FATAL, enable_vulkan_ && !enable_shared_image_)
      << "--webview-enable-vulkan only works with shared image "
         "(--webview-enable-shared-image).";
  LOG_IF(FATAL, enable_vulkan_ && !renderer_settings_.use_skia_renderer)
      << "--webview-enable-vulkan only works with skia renderer "
         "(--enable-features=UseSkiaRenderer).";

  InitializeContext();
}
OutputSurfaceProviderWebview::~OutputSurfaceProviderWebview() = default;

void OutputSurfaceProviderWebview::InitializeContext() {
  DCHECK(!gl_surface_) << "InitializeContext() called twice";
  gl_surface_ = base::MakeRefCounted<AwGLSurface>();

  if (renderer_settings_.use_skia_renderer) {
    auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
    gpu::GpuDriverBugWorkarounds workarounds(
        GpuServiceWebView::GetInstance()
            ->gpu_feature_info()
            .enabled_gpu_driver_bug_workarounds);
    auto gl_context = gl::init::CreateGLContext(
        share_group.get(), gl_surface_.get(), gl::GLContextAttribs());
    gl_context->MakeCurrent(gl_surface_.get());

    auto vulkan_context_provider =
        enable_vulkan_ ? AwVulkanContextProvider::GetOrCreateInstance()
                       : nullptr;

    shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
        share_group, gl_surface_, std::move(gl_context),
        false /* use_virtualized_gl_contexts */, base::BindOnce(&OnContextLost),
        GpuServiceWebView::GetInstance()->gpu_preferences().gr_context_type,
        vulkan_context_provider.get());
    if (!enable_vulkan_) {
      auto feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
          workarounds, GpuServiceWebView::GetInstance()->gpu_feature_info());
      shared_context_state_->InitializeGL(
          GpuServiceWebView::GetInstance()->gpu_preferences(),
          std::move(feature_info));
    }
    shared_context_state_->InitializeGrContext(workarounds,
                                               nullptr /* gr_shader_cache */);
  }
}

std::unique_ptr<viz::OutputSurface>
OutputSurfaceProviderWebview::CreateOutputSurface() {
  DCHECK(gl_surface_)
      << "InitializeContext() must be called before CreateOutputSurface()";

  if (renderer_settings_.use_skia_renderer) {
    auto skia_dependency = std::make_unique<SkiaOutputSurfaceDependencyWebView>(
        TaskQueueWebView::GetInstance(), GpuServiceWebView::GetInstance(),
        shared_context_state_.get(), gl_surface_.get());
    return viz::SkiaOutputSurfaceImpl::Create(std::move(skia_dependency),
                                              renderer_settings_);
  } else {
    auto context_provider = AwRenderThreadContextProvider::Create(
        gl_surface_, DeferredGpuCommandService::GetInstance());
    return std::make_unique<ParentOutputSurface>(gl_surface_,
                                                 std::move(context_provider));
  }
}

}  // namespace android_webview
