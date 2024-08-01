// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/output_surface_provider_webview.h"

#include <utility>

#include "android_webview/browser/gfx/aw_gl_surface_external_stencil.h"
#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"
#include "android_webview/browser/gfx/task_queue_webview.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {

using GLSurfaceContextPair =
    std::pair<scoped_refptr<gl::GLSurface>, scoped_refptr<gl::GLContext>>;

GLSurfaceContextPair GetRealContextForVulkan() {
  // TODO(crbug.com/40155015): Remove all of this after code no longer expects
  // GL to be present (eg for getting capabilities or calling glGetError).
  static base::NoDestructor<base::WeakPtr<gl::GLSurface>> cached_surface;
  static base::NoDestructor<base::WeakPtr<gl::GLContext>> cached_context;

  scoped_refptr<gl::GLSurface> surface = cached_surface.get()->get();
  scoped_refptr<gl::GLContext> context = cached_context.get()->get();
  if (surface && context)
    return std::make_pair(std::move(surface), std::move(context));

  surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                               gfx::Size(1, 1));
  DCHECK(surface);
  // Allow context and surface to be null and just fallback to
  // not having any real EGL context in that case instead of crashing.
  if (surface) {
    gl::GLContextAttribs attribs;

    // This context is used on the GPU thread. We must avoid it being put in a
    // virtualization group with contexts that Chrome creates and uses on other
    // threads to avoid EGL_BAD_ACCESS errors when ANGLE tries to make the
    // underlying native context current on multiple threads simultaneously.
    attribs.angle_context_virtualization_group_number =
        gl::AngleContextVirtualizationGroup::kWebViewRenderThread;

    context = gl::init::CreateGLContext(nullptr, surface.get(), attribs);
  }
  DCHECK(context);

  if (surface)
    *cached_surface.get() = surface->AsWeakPtr();
  if (context)
    *cached_context.get() = context->AsWeakPtr();
  return std::make_pair(std::move(surface), std::move(context));
}

void OnContextLost(std::unique_ptr<bool> expect_loss,
                   bool synthetic_loss,
                   gpu::error::ContextLostReason context_lost_reason) {
  if (expect_loss && *expect_loss)
    return;

  static ::crash_reporter::CrashKeyString<10> reason_key(
      crash_keys::kContextLossReason);
  reason_key.Set(base::NumberToString(static_cast<int>(context_lost_reason)));

  // TODO(crbug.com/40143203): Debugging contexts losts. WebView will
  // intentionally crash in HardwareRenderer::OnViz::DisplayOutputSurface
  // that will happen after this callback. That crash happens on viz thread and
  // doesn't have any useful information. Crash here on RenderThread to
  // understand the reason of context losts.
  // If this implementation changes, need to ensure `expect_loss` access from
  // MarkAllowContextLoss is still valid.
  LOG(FATAL) << "Non owned context lost!";
}

}  // namespace

OutputSurfaceProviderWebView::OutputSurfaceProviderWebView(
    AwVulkanContextProvider* vulkan_context_provider)
    : vulkan_context_provider_(vulkan_context_provider) {
  // Should be kept in sync with compositor_impl_android.cc.
  renderer_settings_.allow_antialiasing = false;
  renderer_settings_.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  renderer_settings_.should_clear_root_render_pass = false;

  enable_vulkan_ = features::IsUsingVulkan();
  DCHECK(!enable_vulkan_ || vulkan_context_provider_);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  debug_settings_.tint_composited_content =
      command_line->HasSwitch(switches::kTintCompositedContent);

  InitializeContext();
}

OutputSurfaceProviderWebView::~OutputSurfaceProviderWebView() {
  // We must destroy |gl_surface_| before |shared_context_state_|, so we will
  // still have context. Note that with ANGLE we are not actually guaranteed to
  // have a current context at this point, so ensure that it is current here (if
  // not using ANGLE, RenderThreadManager::DestroyHardwareRendererOnRT() ensures
  // that there is a current context via its creation of a
  // ScopedAppGLStateRestoreImpl instance, which creates a placeholder context).
  // NOTE: |shared_context_state_| holds a ref to surface, but it explicitly
  // drops it before releasing the context.
  if (gl_surface_->is_angle()) {
    shared_context_state_->MakeCurrent(nullptr);
  }
  // Given this surface is held by gl::GLContext as a default surface, releasing
  // it here doesn't result in destruction of the GL objects (namely the stencil
  // buffer) when it's released. As a result, when the surface is finally
  // destroyed (happens when the context that also holds that is destroyed), the
  // stencil buffer is destroyed on a wrong context resulting in a no context
  // crash. Thus, explicitly ask to destroy the fb here.
  gl_surface_->DestroyExternalStencilFramebuffer();
  gl_surface_.reset();
}

void OutputSurfaceProviderWebView::InitializeContext() {
  DCHECK(!gl_surface_) << "InitializeContext() called twice";
  gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  // If EGL supports EGL_ANGLE_external_context_and_surface, then we will create
  // an ANGLE context for the current native GL context.
  const bool is_angle =
      !enable_vulkan_ && display->ext->b_EGL_ANGLE_external_context_and_surface;

  GLSurfaceContextPair real_context;
  if (enable_vulkan_) {
    DCHECK(!is_angle);
    real_context = GetRealContextForVulkan();
    gl_surface_ =
        base::MakeRefCounted<AwGLSurface>(display, real_context.first);
  } else {
    // We need to draw to FBO for External Stencil support with SkiaRenderer
    gl_surface_ =
        base::MakeRefCounted<AwGLSurfaceExternalStencil>(display, is_angle);
  }

  bool result = gl_surface_->Initialize(gl::GLSurfaceFormat());
  DCHECK(result);

  scoped_refptr<gl::GLContext> gl_context;
  gpu::GpuDriverBugWorkarounds workarounds(
      GpuServiceWebView::GetInstance()
          ->gpu_feature_info()
          .enabled_gpu_driver_bug_workarounds);

  // The SharedContextState expect to receive a GLSurface that was used to
  // create the GLContext. In case of Vulkan, a GLContext is passed, but
  // |gl_surface| is a AWGLSurface, which wraps the real GLSurface that was
  // used to create a context.
  scoped_refptr<gl::GLSurface> gl_surface_for_scs;
  // If failed to create real context for vulkan, just fallback to using
  // GLNonOwnedContext instead of crashing.
  if (enable_vulkan_ && real_context.second) {
    gl_context = std::move(real_context.second);
    gl_surface_for_scs = std::move(real_context.first);
  } else {
    auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
    gl::GLContextAttribs attribs;
    // For ANGLE EGL, we need to create ANGLE context from the current native
    // EGL context and restore state of the native EGL context when releasing
    // the ANGLE context.
    attribs.angle_create_from_external_context = is_angle;

    // By default client arrays are disabled as they are not supported by
    // Chrome's IPC architecture. However, they are required for WebView's
    // usage (in particular, for supporting complex clips).
    attribs.allow_client_arrays = true;

    // Skip validation when dcheck is off.
#if DCHECK_IS_ON()
    attribs.can_skip_validation = false;
#else
    attribs.can_skip_validation = true;
#endif
    gl_context = gl::init::CreateGLContext(share_group.get(), gl_surface_.get(),
                                           attribs);
    gl_context->MakeCurrent(gl_surface_.get());
    gl_surface_for_scs = gl_surface_;
  }

  auto* share_group = gl_context->share_group();
  auto expect_context_loss_ptr = std::make_unique<bool>(false);
  expect_context_loss_ = expect_context_loss_ptr.get();
  shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
      share_group, std::move(gl_surface_for_scs), std::move(gl_context),
      false /* use_virtualized_gl_contexts */,
      base::BindOnce(&OnContextLost, std::move(expect_context_loss_ptr)),
      GpuServiceWebView::GetInstance()->gpu_preferences().gr_context_type,
      vulkan_context_provider_);
  if (!enable_vulkan_) {
    auto feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
        workarounds, GpuServiceWebView::GetInstance()->gpu_feature_info());
    shared_context_state_->InitializeGL(
        GpuServiceWebView::GetInstance()->gpu_preferences(),
        std::move(feature_info));
  }

  shared_context_state_->InitializeSkia(
      GpuServiceWebView::GetInstance()->gpu_preferences(), workarounds);
}

std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
OutputSurfaceProviderWebView::CreateDisplayController() {
  DCHECK(gl_surface_)
      << "InitializeContext() must be called before CreateOutputSurface()";

  auto skia_dependency = std::make_unique<SkiaOutputSurfaceDependencyWebView>(
      TaskQueueWebView::GetInstance(), GpuServiceWebView::GetInstance(),
      shared_context_state_.get(), gl_surface_.get(), vulkan_context_provider_);
  return std::make_unique<viz::DisplayCompositorMemoryAndTaskController>(
      std::move(skia_dependency));
}

std::unique_ptr<viz::OutputSurface>
OutputSurfaceProviderWebView::CreateOutputSurface(
    viz::DisplayCompositorMemoryAndTaskController*
        display_compositor_controller) {
  DCHECK(gl_surface_)
      << "InitializeContext() must be called before CreateOutputSurface()";
  DCHECK(display_compositor_controller)
      << "CreateDisplayController() must be called before "
         "CreateOutputSurface()";
  return viz::SkiaOutputSurfaceImpl::Create(
      display_compositor_controller, renderer_settings_, debug_settings());
}

void OutputSurfaceProviderWebView::MarkAllowContextLoss() {
  // This is safe because either the OnContextLost callback has run and we've
  // already crashed or it has not run and this pointer is still valid.
  if (expect_context_loss_)
    *expect_context_loss_ = true;
  expect_context_loss_ = nullptr;
}

}  // namespace android_webview
