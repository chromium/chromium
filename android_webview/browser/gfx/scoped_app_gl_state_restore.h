// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_H_

#include <memory>
#include <vector>

namespace android_webview {

struct StencilState {
  unsigned char stencil_test_enabled;
  int stencil_front_func;
  int stencil_front_ref;
  int stencil_front_mask;
  int stencil_back_func;
  int stencil_back_ref;
  int stencil_back_mask;
  int stencil_clear;
  int stencil_front_writemask;
  int stencil_back_writemask;
  int stencil_front_fail_op;
  int stencil_front_z_fail_op;
  int stencil_front_z_pass_op;
  int stencil_back_fail_op;
  int stencil_back_z_fail_op;
  int stencil_back_z_pass_op;
};

// This class is not thread safe and should only be used on the UI thread.
//
// Lifetime: Temporary
class ScopedAppGLStateRestore {
 public:
  enum CallMode {
    MODE_DRAW,
    MODE_RESOURCE_MANAGEMENT,
  };

  static ScopedAppGLStateRestore* Current();

  ScopedAppGLStateRestore(CallMode mode, bool save_restore);

  ScopedAppGLStateRestore(const ScopedAppGLStateRestore&) = delete;
  ScopedAppGLStateRestore& operator=(const ScopedAppGLStateRestore&) = delete;

  ~ScopedAppGLStateRestore();

  StencilState stencil_state() const;
  int framebuffer_binding_ext() const;

  // Android HWUI has a bug that asks functor to draw into a 8-bit mask for
  // functionality that is not related to and not needed by webview.
  // Skip this draw which is doing extra unnecessary work.
  bool skip_draw() const;

  class Impl {
   public:
    Impl();
    virtual ~Impl();

    const StencilState& stencil_state() const { return stencil_state_; }
    int framebuffer_binding_ext() const { return framebuffer_binding_ext_; }
    bool skip_draw() const { return skip_draw_; }

   protected:
    StencilState stencil_state_{};
    int framebuffer_binding_ext_ = 0;
    bool skip_draw_ = false;
  };

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_SCOPED_APP_GL_STATE_RESTORE_H_
