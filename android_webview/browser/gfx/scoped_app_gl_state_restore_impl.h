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
  explicit ScopedAppGLStateRestoreImpl(ScopedAppGLStateRestore::CallMode mode);

  ScopedAppGLStateRestoreImpl(const ScopedAppGLStateRestoreImpl&) = delete;
  ScopedAppGLStateRestoreImpl& operator=(const ScopedAppGLStateRestoreImpl&) =
      delete;

  ~ScopedAppGLStateRestoreImpl() override;

 private:
  void SaveHWUIState();
  void RestoreHWUIState();

  const ScopedAppGLStateRestore::CallMode mode_;


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

  GLboolean multisample_enabled_;
  // ARM_shader_framebuffer_fetch
  GLboolean fetch_per_sample_arm_enabled_;
};

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_IMPL_H_
