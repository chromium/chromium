// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_H_

#include <memory>
#include <vector>

#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {
namespace internal {

// Lifetime: Temporary
class ScopedAppGLStateRestoreImpl : public ScopedAppGLStateRestore::Impl {
 public:
  ScopedAppGLStateRestoreImpl(ScopedAppGLStateRestore::CallMode mode,
                              bool save_restore);

  ScopedAppGLStateRestoreImpl(const ScopedAppGLStateRestoreImpl&) = delete;
  ScopedAppGLStateRestoreImpl& operator=(const ScopedAppGLStateRestoreImpl&) =
      delete;

  ~ScopedAppGLStateRestoreImpl() override;

 private:
  void SaveHWUIState(bool save_restore);
  void RestoreHWUIState(bool save_restore);

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
    raw_ptr<GLvoid> pointer;
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
  GLboolean multisample_enabled_;
  // ARM_shader_framebuffer_fetch
  GLboolean fetch_per_sample_arm_enabled_;

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

  struct TextureBindings {
    GLint texture_2d;
    GLint texture_cube_map;
    GLint texture_external_oes;
    // TODO(boliu): TEXTURE_RECTANGLE_ARB
  };

  std::vector<TextureBindings> texture_bindings_;

  GLint vertex_array_bindings_oes_;
};

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_H_
