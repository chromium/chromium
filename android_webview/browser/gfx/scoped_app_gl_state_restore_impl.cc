// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/scoped_app_gl_state_restore_impl.h"

#include <string>

#include "base/android/android_info.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/features.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {

// "App" context is a bit of a stretch. Basically we use this context while
// saving and restoring the App GL state.
class AppContextSurface {
 public:
  AppContextSurface()
      : surface_(new gl::GLSurfaceStub()),
        context_(gl::init::CreateGLContext(nullptr,
                                           surface_.get(),
                                           gl::GLContextAttribs())) {}

  AppContextSurface(const AppContextSurface&) = delete;
  AppContextSurface& operator=(const AppContextSurface&) = delete;

  void MakeCurrent() { context_->MakeCurrent(surface_.get()); }

 private:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
};

base::LazyInstance<AppContextSurface>::DestructorAtExit g_app_context_surface =
    LAZY_INSTANCE_INITIALIZER;

// Make the global g_app_context_surface current so that the gl_binding is not
// NULL for making gl* calls. The binding can be null if another GlContext was
// destroyed immediately before gl* calls here.
void MakeAppContextCurrent() {
  g_app_context_surface.Get().MakeCurrent();
}

void GLEnableDisable(GLenum cap, bool enable) {
  if (enable)
    glEnable(cap);
  else
    glDisable(cap);
}

bool ClearGLErrors(bool warn, const char* msg) {
  bool no_error = true;
  GLenum error;
  CHECK(gl::g_current_gl_context);
  while ((error = glGetError()) != GL_NO_ERROR) {
    DLOG_IF(WARNING, warn) << error << " " << msg;
    no_error = false;
  }

  return no_error;
}

bool g_globals_initialized = false;
GLint g_gl_max_texture_units = 0;
GLint g_gl_max_vertex_attribs = 0;
bool g_supports_arm_shader_framebuffer_fetch = false;
bool g_supports_nv_concervative_raster = false;
bool g_supports_disable_multisample = false;

}  // namespace

namespace internal {

ScopedAppGLStateRestoreImpl::ScopedAppGLStateRestoreImpl(
    ScopedAppGLStateRestore::CallMode mode)
    : mode_(mode) {
  MakeAppContextCurrent();

  ClearGLErrors(true, "Incoming GLError");

  if (mode_ == ScopedAppGLStateRestore::MODE_DRAW &&
      base::android::android_info::sdk_int() ==
          base::android::android_info::SDK_VERSION_S) {
    GLint red_bits = 0;
    GLint green_bits = 0;
    GLint blue_bits = 0;
    GLint alpha_bits = 0;
    glGetIntegerv(GL_RED_BITS, &red_bits);
    glGetIntegerv(GL_GREEN_BITS, &green_bits);
    glGetIntegerv(GL_BLUE_BITS, &blue_bits);
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
    skip_draw_ =
        red_bits == 8 && green_bits == 0 && blue_bits == 0 && alpha_bits == 0;
  }

  glGetBooleanv(GL_STENCIL_TEST, &stencil_state_.stencil_test_enabled);
  glGetIntegerv(GL_STENCIL_FUNC, &stencil_state_.stencil_front_func);
  glGetIntegerv(GL_STENCIL_VALUE_MASK, &stencil_state_.stencil_front_mask);
  glGetIntegerv(GL_STENCIL_REF, &stencil_state_.stencil_front_ref);
  glGetIntegerv(GL_STENCIL_BACK_FUNC, &stencil_state_.stencil_back_func);
  glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &stencil_state_.stencil_back_mask);
  glGetIntegerv(GL_STENCIL_BACK_REF, &stencil_state_.stencil_back_ref);
  glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil_state_.stencil_clear);
  glGetIntegerv(GL_STENCIL_WRITEMASK, &stencil_state_.stencil_front_writemask);
  glGetIntegerv(GL_STENCIL_BACK_WRITEMASK,
                &stencil_state_.stencil_back_writemask);
  glGetIntegerv(GL_STENCIL_FAIL, &stencil_state_.stencil_front_fail_op);
  glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL,
                &stencil_state_.stencil_front_z_fail_op);
  glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS,
                &stencil_state_.stencil_front_z_pass_op);
  glGetIntegerv(GL_STENCIL_BACK_FAIL, &stencil_state_.stencil_back_fail_op);
  glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL,
                &stencil_state_.stencil_back_z_fail_op);
  glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS,
                &stencil_state_.stencil_back_z_pass_op);

  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_binding_ext_);

  if (!g_globals_initialized) {
    g_globals_initialized = true;

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &g_gl_max_vertex_attribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &g_gl_max_texture_units);

    const char* extensions_string =
        reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    if (extensions_string) {
      gfx::ExtensionSet extensions(gfx::MakeExtensionSet(extensions_string));

      g_supports_arm_shader_framebuffer_fetch =
          extensions.contains("GL_ARM_shader_framebuffer_fetch");
      g_supports_nv_concervative_raster =
          extensions.contains("GL_NV_conservative_raster");
      g_supports_disable_multisample =
          extensions.contains("GL_EXT_multisample_compatibility");
    }
  }

  SaveHWUIState();

  if (mode_ == ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT &&
      ::gl::g_current_gl_driver->fn.glBindRenderbufferEXTFn != nullptr) {
    // Android 5.0.0 specific qualcomm workaround. See crbug.com/434570.
    // Null check the binding since checking the proper condition is hard. See
    // crbug.com/950472.
    glBindRenderbufferEXT(GL_RENDERBUFFER, 0);
  }

  DCHECK(ClearGLErrors(false, NULL));
}

void ScopedAppGLStateRestoreImpl::SaveHWUIState() {
  if (g_supports_arm_shader_framebuffer_fetch)
    glGetBooleanv(GL_FETCH_PER_SAMPLE_ARM, &fetch_per_sample_arm_enabled_);

  if (g_supports_disable_multisample)
    glGetBooleanv(GL_MULTISAMPLE_EXT, &multisample_enabled_);

  vertex_attrib_.resize(g_gl_max_vertex_attribs);
  for (GLint i = 0; i < g_gl_max_vertex_attribs; ++i) {
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                        &vertex_attrib_[i].vertex_attrib_array_buffer_binding);
  }
}

ScopedAppGLStateRestoreImpl::~ScopedAppGLStateRestoreImpl() {
  MakeAppContextCurrent();
  DCHECK(ClearGLErrors(false, NULL));
  RestoreHWUIState();

  // Do not leak GLError out of chromium.
  ClearGLErrors(true, "Chromium GLError");
}

void ScopedAppGLStateRestoreImpl::RestoreHWUIState() {
  // Some state is restored when skia renderer is enabled. This is because newer
  // skia version can modify newer GL state that older versions of Android were
  // not aware of, so did not restore automatically. Note some of these are just
  // resetting back to default state for this reason. This code is currently
  // conservative; it's likely that not all android versions needs all of these
  // states restored.
  if (gl::g_current_gl_driver->fn.glWindowRectanglesEXTFn)
    glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);

  if (g_supports_arm_shader_framebuffer_fetch)
    GLEnableDisable(GL_FETCH_PER_SAMPLE_ARM, fetch_per_sample_arm_enabled_);

  if (g_supports_disable_multisample)
    GLEnableDisable(GL_MULTISAMPLE_EXT, multisample_enabled_);

  // We do restore it even with Skia on the other side because it's new
  // extension that skia on Android P and Q didn't use.
  if (g_supports_nv_concervative_raster)
    glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);

  if (gl::g_current_gl_driver->fn.glVertexAttribDivisorANGLEFn) {
    for (GLint i = 0; i < g_gl_max_vertex_attribs; ++i) {
      glBindBuffer(GL_ARRAY_BUFFER,
                   vertex_attrib_[i].vertex_attrib_array_buffer_binding);
      glVertexAttribDivisorANGLE(i, 0);
    }
  }
  if (gl::g_current_gl_driver->fn.glBindSamplerFn) {
    for (int ii = 0; ii < g_gl_max_texture_units; ++ii) {
      glActiveTexture(GL_TEXTURE0 + ii);
      glBindSampler(ii, 0);
    }
  }
}

}  // namespace internal
}  // namespace android_webview
