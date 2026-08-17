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
#include "android_webview/common/aw_features.h"
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
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {

using GLSurfaceContextPair =
    std::pair<scoped_refptr<gl::GLSurface>, scoped_refptr<gl::GLContext>>;

GLSurfaceContextPair GetRealContextForVulkan() {
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

scoped_refptr<gpu::SharedContextState> CreateSharedContextStateHelper(
    AwVulkanContextProvider* vulkan_context_provider,
    const AwGrContextOptionsProvider* gr_context_options_provider,
    bool enable_vulkan,
    gl::GLSurface* gl_surface,
    GLSurfaceContextPair real_context) {
  gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  const bool is_angle =
      !enable_vulkan && display->ext->b_EGL_ANGLE_external_context_and_surface;

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
  if (enable_vulkan && real_context.second) {
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
    gl_context =
        gl::init::CreateGLContext(share_group.get(), gl_surface, attribs);
    gl_context->MakeCurrent(gl_surface);
    gl_surface_for_scs = gl_surface;
  }

  auto* share_group = gl_context->share_group();
  // Context loss is checked and handled on RenderThread in
  // HardwareRenderer::CrashOnContextLoss(), so no immediate OnContextLost
  // callback handler is required here.
  auto shared_context_state = base::MakeRefCounted<gpu::SharedContextState>(
      share_group, std::move(gl_surface_for_scs), std::move(gl_context),
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
      GpuServiceWebView::GetInstance()->gpu_preferences().gr_context_type,
      vulkan_context_provider, /*dawn_context_provider=*/nullptr,
      /*peak_memory_monitor=*/nullptr,
      /*direct_rendering_display_compositor_enabled=*/false,
      /*created_on_compositor_gpu_thread=*/false, gr_context_options_provider);
  if (!enable_vulkan) {
    shared_context_state->InitializeGL(
        GpuServiceWebView::GetInstance()->gpu_preferences(), workarounds,
        GpuServiceWebView::GetInstance()->gpu_feature_info());
  }

  shared_context_state->InitializeSkia(
      GpuServiceWebView::GetInstance()->gpu_preferences(), workarounds);

  return shared_context_state;
}

scoped_refptr<gpu::SharedContextState> GetOrCreateSharedContextState(
    AwVulkanContextProvider* vulkan_context_provider,
    const AwGrContextOptionsProvider* gr_context_options_provider,
    bool enable_vulkan,
    gl::GLSurface* gl_surface,
    GLSurfaceContextPair real_context) {
  static base::NoDestructor<base::WeakPtr<gpu::SharedContextState>>
      cached_shared_context_state;

  if (*cached_shared_context_state &&
      !(*cached_shared_context_state)->context_lost()) {
    auto* shared_context_state = cached_shared_context_state->get();
    // Validate that the existing SharedContextState has the same properties
    // we would use if creating a new one.
    CHECK_EQ(shared_context_state->IsUsingGL(), !enable_vulkan);
    CHECK_EQ(
        shared_context_state->gr_context_type(),
        GpuServiceWebView::GetInstance()->gpu_preferences().gr_context_type);
    CHECK(!shared_context_state->use_virtualized_gl_contexts());
    if (enable_vulkan) {
      CHECK_EQ(shared_context_state->vk_context_provider(),
               vulkan_context_provider);
    } else {
      gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
      const bool is_angle =
          !enable_vulkan &&
          display->ext->b_EGL_ANGLE_external_context_and_surface;
      CHECK(shared_context_state->feature_info());
      CHECK_EQ(shared_context_state->feature_info()->gl_version_info().is_angle,
               is_angle);
    }
    return base::WrapRefCounted(shared_context_state);
  }

  // To avoid confusion of sharing the first WebView's gl_surface_ globally,
  // we create an independent dummy surface for the SharedContextState.
  scoped_refptr<gl::GLSurface> dummy_surface;
  gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  const bool is_angle =
      !enable_vulkan && display->ext->b_EGL_ANGLE_external_context_and_surface;

  if (enable_vulkan) {
    if (real_context.first) {
      dummy_surface =
          base::MakeRefCounted<AwGLSurface>(display, real_context.first);
    }
  } else {
    // SharedContextState only uses this as a default anchor surface, so
    // a basic AwGLSurface is sufficient instead of AwGLSurfaceExternalStencil.
    dummy_surface = base::MakeRefCounted<AwGLSurface>(display, is_angle);
  }

  if (dummy_surface) {
    dummy_surface->Initialize(gl::GLSurfaceFormat());
  }

  scoped_refptr<gpu::SharedContextState> shared_context_state =
      CreateSharedContextStateHelper(
          vulkan_context_provider, gr_context_options_provider, enable_vulkan,
          dummy_surface ? dummy_surface.get() : gl_surface,
          std::move(real_context));

  *cached_shared_context_state = shared_context_state->GetWeakPtr();
  return shared_context_state;
}

}  // namespace

OutputSurfaceProviderWebView::OutputSurfaceProviderWebView(
    AwVulkanContextProvider* vulkan_context_provider)
    : vulkan_context_provider_(vulkan_context_provider),
      aw_gr_context_options_provider_(
          std::make_unique<AwGrContextOptionsProvider>()) {
  // Should be kept in sync with compositor_impl_android.cc.
  renderer_settings_.allow_antialiasing = false;
  renderer_settings_.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  renderer_settings_.should_clear_root_render_pass = false;

  enable_vulkan_ = ::features::IsUsingVulkan();
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

  if (base::FeatureList::IsEnabled(
          features::kWebViewSingleSharedContextState)) {
    shared_context_state_ = GetOrCreateSharedContextState(
        vulkan_context_provider_, aw_gr_context_options_provider_.get(),
        enable_vulkan_, gl_surface_.get(), std::move(real_context));
    return;
  }

  shared_context_state_ = CreateSharedContextStateHelper(
      vulkan_context_provider_, aw_gr_context_options_provider_.get(),
      enable_vulkan_, gl_surface_.get(), std::move(real_context));
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

}  // namespace android_webview
