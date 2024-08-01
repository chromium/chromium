// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_gl_surface_external_stencil.h"

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "base/feature_list.h"
#include "base/strings/stringize_macros.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_helper.h"

namespace android_webview {

// Lifetime: WebView
class AwGLSurfaceExternalStencil::BlitContext {
 public:
  BlitContext() {
    // NOTE: Quad is flipped vertically.

    // clang-format off
    const GLchar vertex_shader_str[] =
    STRINGIZE(
      attribute vec2 position;
      attribute vec2 texcoords;
      varying vec2 v_texcoords;
      void main()
      {
        v_texcoords = vec2(texcoords.x, 1.0 - texcoords.y);
        vec2 pos = position * 2.0 - vec2(1.0);
        gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);
      }
    );

    const GLchar fragment_shader_str[] =
    STRINGIZE(
      precision mediump float;
      varying vec2 v_texcoords;
      uniform sampler2D texture;
      void main()
      {
        gl_FragColor = texture2D ( texture, v_texcoords );
      }
    );
    // clang-format on

    GLuint vertex_shader =
        gl::GLHelper::LoadShader(GL_VERTEX_SHADER, vertex_shader_str);
    GLuint fragment_shader =
        gl::GLHelper::LoadShader(GL_FRAGMENT_SHADER, fragment_shader_str);

    program_ = glCreateProgram();

    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glBindAttribLocation(program_, 0, "position");
    glBindAttribLocation(program_, 1, "texcoords");

    glLinkProgram(program_);

    GLint linked;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    DCHECK(linked);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    texture_uniform_ = glGetUniformLocation(program_, "texture");
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &gl_max_vertex_attribs_);
  }

  ~BlitContext() { glDeleteProgram(program_); }

  void Bind() {
    // If vertex array objects are supported we need to reset it to default one,
    // so we won't break someones else VAS by changing attributes.
    if (gl::g_current_gl_driver->fn.glBindVertexArrayOESFn) {
      glBindVertexArrayOES(0);
    }

    glUseProgram(program_);
    for (GLint i = 2; i < gl_max_vertex_attribs_; ++i) {
      glDisableVertexAttribArray(i);
    }

    // Note that function is not ANGLE only.
    if (gl::g_current_gl_driver->fn.glVertexAttribDivisorANGLEFn) {
      glVertexAttribDivisorANGLE(0, 0);
      glVertexAttribDivisorANGLE(1, 0);
    }

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUniform1i(texture_uniform_, 0);
  }

 private:
  GLuint program_;
  GLint texture_uniform_;
  GLint gl_max_vertex_attribs_;
};

// Lifetime: WebView
class AwGLSurfaceExternalStencil::FrameBuffer {
 public:
  FrameBuffer(gfx::Size size) : size_(size) {
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffersEXT(1, &frame_buffer_object_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, frame_buffer_object_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture_id_, 0);

    DCHECK(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) ==
           GL_FRAMEBUFFER_COMPLETE)
        << "Failed to set up framebuffer for WebView GL drawing.";
  }

  void Clear() {
    glBindFramebufferEXT(GL_FRAMEBUFFER, frame_buffer_object_);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  ~FrameBuffer() {
    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (frame_buffer_object_)
      glDeleteFramebuffersEXT(1, &frame_buffer_object_);
    if (texture_id_)
      glDeleteTextures(1, &texture_id_);
  }

  GLuint frame_buffer_object() const { return frame_buffer_object_; }

  GLuint texture_id() const { return texture_id_; }

  gfx::Size size() const { return size_; }

 private:
  GLuint frame_buffer_object_ = 0;
  GLuint texture_id_ = 0;
  gfx::Size size_;
};

AwGLSurfaceExternalStencil::AwGLSurfaceExternalStencil(
    gl::GLDisplayEGL* display,
    bool is_angle)
    : AwGLSurface(display, is_angle) {}

AwGLSurfaceExternalStencil::~AwGLSurfaceExternalStencil() {
  InvalidateWeakPtrs();
}

unsigned int AwGLSurfaceExternalStencil::GetBackingFramebufferObject() {
  const auto& stencil_state =
      android_webview::ScopedAppGLStateRestore::Current()->stencil_state();

  if (stencil_state.stencil_test_enabled) {
    // Framebuffer was created during RecalculateClipAndTransform();
    DCHECK(framebuffer_);
    return framebuffer_->frame_buffer_object();
  }

  return AwGLSurface::GetBackingFramebufferObject();
}

gfx::SwapResult AwGLSurfaceExternalStencil::SwapBuffers(
    PresentationCallback callback,
    gfx::FrameData frame_data) {
  const auto& stencil_state =
      android_webview::ScopedAppGLStateRestore::Current()->stencil_state();

  if (stencil_state.stencil_test_enabled) {
    DCHECK(framebuffer_);
    DCHECK(blit_context_);

    // Flush skia renderer rendering. This is working around what appears to be
    // a driver bug that causes rendering to break.
    glFlush();

    // Bind required context.
    blit_context_->Bind();

    // Bind real frame buffer.
    glBindFramebufferEXT(GL_FRAMEBUFFER,
                         AwGLSurface::GetBackingFramebufferObject());

    // Restore stencil state.
    glEnable(GL_STENCIL_TEST);
    glStencilFuncSeparate(GL_FRONT, stencil_state.stencil_front_func,
                          stencil_state.stencil_front_ref,
                          stencil_state.stencil_front_mask);
    glStencilFuncSeparate(GL_BACK, stencil_state.stencil_back_func,
                          stencil_state.stencil_back_ref,
                          stencil_state.stencil_back_mask);
    glStencilMaskSeparate(GL_FRONT, stencil_state.stencil_front_writemask);
    glStencilMaskSeparate(GL_BACK, stencil_state.stencil_back_writemask);
    glStencilOpSeparate(GL_FRONT, stencil_state.stencil_front_fail_op,
                        stencil_state.stencil_front_z_fail_op,
                        stencil_state.stencil_front_z_pass_op);
    glStencilOpSeparate(GL_BACK, stencil_state.stencil_back_fail_op,
                        stencil_state.stencil_back_z_fail_op,
                        stencil_state.stencil_back_z_pass_op);

    // Scale clip rect to (0, 0)x(1, 1) space.
    gfx::QuadF quad = gfx::QuadF(gfx::RectF(clip_rect_));
    quad.Scale(1.0 / viewport_.width(), 1.0 / viewport_.height());

    gfx::QuadF tex_quad = gfx::QuadF(gfx::RectF(1.0, 1.0));

    // Set-up vertex attributes. p1-p4-p2-p3 forms triangle strip for quad.
    // clang-format off
    const gfx::PointF data[8] = {quad.p1(), tex_quad.p1(),
                                 quad.p4(), tex_quad.p4(),
                                 quad.p2(), tex_quad.p2(),
                                 quad.p3(), tex_quad.p3()};

    // clang-format on
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gfx::PointF) * 2,
                          &data[1]);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(gfx::PointF) * 2,
                          &data[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer_->texture_id());
    if (gl::g_current_gl_driver->fn.glBindSamplerFn)
      glBindSampler(0, 0);

    // We need to restore viewport as it might have changed by renderer
    glViewport(0, 0, viewport_.width(), viewport_.height());

    // We draw only inside clip rect, no need to scissor.
    glDisable(GL_SCISSOR_TEST);

    // Restore color mask in case.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Restore blending.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);

    if (gl::g_current_gl_driver->fn.glWindowRectanglesEXTFn)
      glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  return AwGLSurface::SwapBuffers(std::move(callback), std::move(frame_data));
}

void AwGLSurfaceExternalStencil::RecalculateClipAndTransform(
    gfx::Size* viewport,
    gfx::Rect* clip_rect,
    gfx::Transform* transform) {
  clip_rect_ = *clip_rect;
  viewport_ = *viewport;

  const auto& stencil_state =
      android_webview::ScopedAppGLStateRestore::Current()->stencil_state();
  if (stencil_state.stencil_test_enabled) {
    // Initialize graphics part needed for blit with stencil here so we don't
    // change any gl state after GrContext reset after this function call.
    if (!blit_context_) {
      blit_context_ = std::make_unique<BlitContext>();
    }

    if (!framebuffer_ || framebuffer_->size() != clip_rect_.size()) {
      // Delete old one first to reduce peak memory.
      framebuffer_.reset();
      framebuffer_ = std::make_unique<FrameBuffer>(clip_rect_.size());
    }

    framebuffer_->Clear();

    // Adjust transform, clip rect and viewport to be in original clip rect
    // space as we will draw to FBO of clip_rect size and blit it to screen at
    // original location.
    transform->PostTranslate(-clip_rect->x(), -clip_rect->y());
    clip_rect->set_origin(gfx::Point(0, 0));
    *viewport = clip_rect_.size();
  } else {
    // We're not going to draw to frame buffer, so we can free it to save
    // memory, assuming |stencil_test_enabled| doesn't change often.
    framebuffer_.reset();
  }
}

bool AwGLSurfaceExternalStencil::IsDrawingToFBO() {
  const auto& stencil_state =
      android_webview::ScopedAppGLStateRestore::Current()->stencil_state();
  return stencil_state.stencil_test_enabled;
}

void AwGLSurfaceExternalStencil::DestroyExternalStencilFramebuffer() {
  framebuffer_.reset();
  blit_context_.reset();
}

}  // namespace android_webview
