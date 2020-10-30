// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
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
      : surface(new gl::GLSurfaceStub),
        context(gl::init::CreateGLContext(nullptr,
                                          surface.get(),
                                          gl::GLContextAttribs())) {}
  void MakeCurrent() { context->MakeCurrent(surface.get()); }

 private:
  scoped_refptr<gl::GLSurfaceStub> surface;
  scoped_refptr<gl::GLContext> context;

  DISALLOW_COPY_AND_ASSIGN(AppContextSurface);
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
  while ((error = glGetError()) != GL_NO_ERROR) {
    DLOG_IF(WARNING, warn) << error << " " << msg;
    no_error = false;
  }

  return no_error;
}

bool g_globals_initialized = false;
GLint g_gl_max_texture_units = 0;
GLint g_gl_max_vertex_attribs = 0;
bool g_supports_oes_vertex_array_object = false;
ScopedAppGLStateRestore* g_current_instance = nullptr;

}  // namespace

namespace internal {

class ScopedAppGLStateRestoreImpl {
 public:
  ScopedAppGLStateRestoreImpl(ScopedAppGLStateRestore::CallMode mode,
                              bool save_restore);
  ~ScopedAppGLStateRestoreImpl();

  StencilState stencil_state() const { return stencil_state_; }
  GLint framebuffer_binding_ext() const { return framebuffer_binding_ext_; }

 private:
  void SaveHWUIState();
  void RestoreHWUIState();

  const ScopedAppGLStateRestore::CallMode mode_;
  const bool save_restore_;

  GLint pack_alignment_;
  GLint unpack_alignment_;

  struct VertexAttributes {
    GLint enabled;
    GLint size;
    GLint type;
    GLint normalized;
    GLint stride;
    GLvoid* pointer;
    GLint vertex_attrib_array_buffer_binding;
    GLfloat current_vertex_attrib[4];
  };
  std::vector<VertexAttributes> vertex_attrib_;

  GLint vertex_array_buffer_binding_;
  GLint index_array_buffer_binding_;

  GLboolean depth_test_;
  GLboolean cull_face_;
  GLint cull_face_mode_;
  GLboolean color_mask_[4];
  GLfloat color_clear_[4];
  GLfloat blend_color_[4];
  GLfloat depth_clear_;
  GLint current_program_;
  GLint depth_func_;
  GLboolean depth_mask_;
  GLfloat depth_rage_[2];
  GLint front_face_;
  GLint hint_generate_mipmap_;
  GLfloat line_width_;
  GLfloat polygon_offset_factor_;
  GLfloat polygon_offset_units_;
  GLfloat sample_coverage_value_;
  GLboolean sample_coverage_invert_;
  GLint blend_equation_rgb_;
  GLint blend_equation_alpha_;

  GLboolean enable_dither_;
  GLboolean enable_polygon_offset_fill_;
  GLboolean enable_sample_alpha_to_coverage_;
  GLboolean enable_sample_coverage_;

  // Not saved/restored in MODE_DRAW.
  GLboolean blend_enabled_;
  GLint blend_src_rgb_;
  GLint blend_src_alpha_;
  GLint blend_dest_rgb_;
  GLint blend_dest_alpha_;
  GLint active_texture_;
  GLint viewport_[4];
  GLboolean scissor_test_;
  GLint scissor_box_[4];

  StencilState stencil_state_;

  GLint framebuffer_binding_ext_;

  struct TextureBindings {
    GLint texture_2d;
    GLint texture_cube_map;
    GLint texture_external_oes;
    // TODO(boliu): TEXTURE_RECTANGLE_ARB
  };

  std::vector<TextureBindings> texture_bindings_;

  GLint vertex_array_bindings_oes_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppGLStateRestoreImpl);
};

ScopedAppGLStateRestoreImpl::ScopedAppGLStateRestoreImpl(
    ScopedAppGLStateRestore::CallMode mode,
    bool save_restore)
    : mode_(mode), save_restore_(save_restore) {
  TRACE_EVENT0("android_webview", "AppGLStateSave");
  MakeAppContextCurrent();

  ClearGLErrors(true, "Incoming GLError");

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

  glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer_binding_ext_);

  if (save_restore_) {
    SaveHWUIState();
  }

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
  if (!g_globals_initialized) {
    g_globals_initialized = true;

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &g_gl_max_vertex_attribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &g_gl_max_texture_units);

    std::string extensions;
    const char* extensions_c_str =
        reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions_c_str)
      extensions = extensions_c_str;
    g_supports_oes_vertex_array_object =
        extensions.find("GL_OES_vertex_array_object") != std::string::npos;
  }

  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vertex_array_buffer_binding_);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_array_buffer_binding_);

  switch (mode_) {
    case ScopedAppGLStateRestore::MODE_DRAW:
      DCHECK_EQ(0, vertex_array_buffer_binding_);
      DCHECK_EQ(0, index_array_buffer_binding_);
      break;
    case ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT:
      glGetBooleanv(GL_BLEND, &blend_enabled_);
      glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb_);
      glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha_);
      glGetIntegerv(GL_BLEND_DST_RGB, &blend_dest_rgb_);
      glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dest_alpha_);
      glGetIntegerv(GL_VIEWPORT, viewport_);
      glGetBooleanv(GL_SCISSOR_TEST, &scissor_test_);
      glGetIntegerv(GL_SCISSOR_BOX, scissor_box_);
      break;
  }

  glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment_);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment_);

  glGetBooleanv(GL_DEPTH_TEST, &depth_test_);
  glGetBooleanv(GL_CULL_FACE, &cull_face_);
  glGetIntegerv(GL_CULL_FACE_MODE, &cull_face_mode_);
  glGetBooleanv(GL_COLOR_WRITEMASK, color_mask_);
  glGetIntegerv(GL_CURRENT_PROGRAM, &current_program_);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, color_clear_);
  glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_clear_);
  glGetFloatv(GL_BLEND_COLOR, blend_color_);
  glGetIntegerv(GL_DEPTH_FUNC, &depth_func_);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_);
  glGetFloatv(GL_DEPTH_RANGE, depth_rage_);
  glGetIntegerv(GL_FRONT_FACE, &front_face_);
  glGetIntegerv(GL_GENERATE_MIPMAP_HINT, &hint_generate_mipmap_);
  glGetFloatv(GL_LINE_WIDTH, &line_width_);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygon_offset_factor_);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygon_offset_units_);
  glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, &sample_coverage_value_);
  glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &sample_coverage_invert_);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_equation_rgb_);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_equation_alpha_);

  glGetBooleanv(GL_DITHER, &enable_dither_);
  glGetBooleanv(GL_POLYGON_OFFSET_FILL, &enable_polygon_offset_fill_);
  glGetBooleanv(GL_SAMPLE_ALPHA_TO_COVERAGE, &enable_sample_alpha_to_coverage_);
  glGetBooleanv(GL_SAMPLE_COVERAGE, &enable_sample_coverage_);

  glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);

  texture_bindings_.resize(g_gl_max_texture_units);
  for (int ii = 0; ii < g_gl_max_texture_units; ++ii) {
    glActiveTexture(GL_TEXTURE0 + ii);
    TextureBindings& bindings = texture_bindings_[ii];
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings.texture_2d);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bindings.texture_cube_map);
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES,
                  &bindings.texture_external_oes);
  }

  if (g_supports_oes_vertex_array_object) {
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &vertex_array_bindings_oes_);
    glBindVertexArrayOES(0);
  }

  vertex_attrib_.resize(g_gl_max_vertex_attribs);
  for (GLint i = 0; i < g_gl_max_vertex_attribs; ++i) {
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vertex_attrib_[i].enabled);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vertex_attrib_[i].size);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vertex_attrib_[i].type);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vertex_attrib_[i].normalized);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vertex_attrib_[i].stride);
    glGetVertexAttribPointerv(
        i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vertex_attrib_[i].pointer);
    glGetVertexAttribiv(i,
                        GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                        &vertex_attrib_[i].vertex_attrib_array_buffer_binding);
    glGetVertexAttribfv(
        i, GL_CURRENT_VERTEX_ATTRIB, vertex_attrib_[i].current_vertex_attrib);
  }
}

ScopedAppGLStateRestoreImpl::~ScopedAppGLStateRestoreImpl() {
  TRACE_EVENT0("android_webview", "AppGLStateRestore");
  MakeAppContextCurrent();
  if (save_restore_) {
    DCHECK(ClearGLErrors(false, NULL));
    RestoreHWUIState();
  }

  // Do not leak GLError out of chromium.
  ClearGLErrors(true, "Chromium GLError");
}

void ScopedAppGLStateRestoreImpl::RestoreHWUIState() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer_binding_ext_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_buffer_binding_);

  if (g_supports_oes_vertex_array_object)
    glBindVertexArrayOES(0);

  for (GLint i = 0; i < g_gl_max_vertex_attribs; ++i) {
    glBindBuffer(GL_ARRAY_BUFFER,
                 vertex_attrib_[i].vertex_attrib_array_buffer_binding);
    glVertexAttribPointer(i,
                          vertex_attrib_[i].size,
                          vertex_attrib_[i].type,
                          vertex_attrib_[i].normalized,
                          vertex_attrib_[i].stride,
                          vertex_attrib_[i].pointer);

    glVertexAttrib4fv(i, vertex_attrib_[i].current_vertex_attrib);

    if (vertex_attrib_[i].enabled) {
      glEnableVertexAttribArray(i);
    } else {
      glDisableVertexAttribArray(i);
    }
  }

  if (g_supports_oes_vertex_array_object && vertex_array_bindings_oes_ != 0)
    glBindVertexArrayOES(vertex_array_bindings_oes_);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_array_buffer_binding_);

  for (int ii = 0; ii < g_gl_max_texture_units; ++ii) {
    glActiveTexture(GL_TEXTURE0 + ii);
    TextureBindings& bindings = texture_bindings_[ii];
    glBindTexture(GL_TEXTURE_2D, bindings.texture_2d);
    glBindTexture(GL_TEXTURE_CUBE_MAP, bindings.texture_cube_map);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, bindings.texture_external_oes);
  }
  glActiveTexture(active_texture_);

  glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment_);

  GLEnableDisable(GL_DEPTH_TEST, depth_test_);

  GLEnableDisable(GL_CULL_FACE, cull_face_);
  glCullFace(cull_face_mode_);

  glColorMask(color_mask_[0], color_mask_[1], color_mask_[2], color_mask_[3]);

  glUseProgram(current_program_);

  glClearColor(
      color_clear_[0], color_clear_[1], color_clear_[2], color_clear_[3]);
  glBlendColor(
      blend_color_[0], blend_color_[1], blend_color_[2], blend_color_[3]);
  glClearDepth(depth_clear_);
  glDepthFunc(depth_func_);
  glDepthMask(depth_mask_);
  glDepthRange(depth_rage_[0], depth_rage_[1]);
  glFrontFace(front_face_);
  glHint(GL_GENERATE_MIPMAP_HINT, hint_generate_mipmap_);
  // TODO(boliu): GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES ??
  glLineWidth(line_width_);
  glPolygonOffset(polygon_offset_factor_, polygon_offset_units_);
  glSampleCoverage(sample_coverage_value_, sample_coverage_invert_);
  glBlendEquationSeparate(blend_equation_rgb_, blend_equation_alpha_);

  GLEnableDisable(GL_DITHER, enable_dither_);
  GLEnableDisable(GL_POLYGON_OFFSET_FILL, enable_polygon_offset_fill_);
  GLEnableDisable(GL_SAMPLE_ALPHA_TO_COVERAGE,
                  enable_sample_alpha_to_coverage_);
  GLEnableDisable(GL_SAMPLE_COVERAGE, enable_sample_coverage_);

  switch(mode_) {
    case ScopedAppGLStateRestore::MODE_DRAW:
      // No-op.
      break;
    case ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT:
      GLEnableDisable(GL_BLEND, blend_enabled_);
      glBlendFuncSeparate(
          blend_src_rgb_, blend_dest_rgb_, blend_src_alpha_, blend_dest_alpha_);

      glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);

      GLEnableDisable(GL_SCISSOR_TEST, scissor_test_);

      glScissor(
          scissor_box_[0], scissor_box_[1], scissor_box_[2], scissor_box_[3]);
      break;
  }

  GLEnableDisable(GL_STENCIL_TEST, stencil_state_.stencil_test_enabled);
  glStencilFuncSeparate(GL_FRONT, stencil_state_.stencil_front_func,
                        stencil_state_.stencil_front_mask,
                        stencil_state_.stencil_front_ref);
  glStencilFuncSeparate(GL_BACK, stencil_state_.stencil_back_func,
                        stencil_state_.stencil_back_mask,
                        stencil_state_.stencil_back_ref);
  glClearStencil(stencil_state_.stencil_clear);
  glStencilMaskSeparate(GL_FRONT, stencil_state_.stencil_front_writemask);
  glStencilMaskSeparate(GL_BACK, stencil_state_.stencil_back_writemask);
  glStencilOpSeparate(GL_FRONT, stencil_state_.stencil_front_fail_op,
                      stencil_state_.stencil_front_z_fail_op,
                      stencil_state_.stencil_front_z_pass_op);
  glStencilOpSeparate(GL_BACK, stencil_state_.stencil_back_fail_op,
                      stencil_state_.stencil_back_z_fail_op,
                      stencil_state_.stencil_back_z_pass_op);
}

}  // namespace internal

// static
ScopedAppGLStateRestore* ScopedAppGLStateRestore::Current() {
  DCHECK(g_current_instance);
  return g_current_instance;
}

ScopedAppGLStateRestore::ScopedAppGLStateRestore(CallMode mode,
                                                 bool save_restore)
    : impl_(new internal::ScopedAppGLStateRestoreImpl(mode, save_restore)) {
  DCHECK(!g_current_instance);
  g_current_instance = this;
}

ScopedAppGLStateRestore::~ScopedAppGLStateRestore() {
  DCHECK_EQ(this, g_current_instance);
  g_current_instance = nullptr;
}

StencilState ScopedAppGLStateRestore::stencil_state() const {
  return impl_->stencil_state();
}
int ScopedAppGLStateRestore::framebuffer_binding_ext() const {
  return impl_->framebuffer_binding_ext();
}

}  // namespace android_webview
