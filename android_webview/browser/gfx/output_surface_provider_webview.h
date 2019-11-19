// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_OUTPUT_SURFACE_PROVIDER_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_OUTPUT_SURFACE_PROVIDER_WEBVIEW_H_

#include <memory>

#include "android_webview/browser/gfx/aw_gl_surface.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/display/renderer_settings.h"
#include "gpu/command_buffer/service/shared_context_state.h"

namespace gpu {
class SharedContextState;
}

namespace viz {
class OutputSurface;
}

namespace android_webview {

class OutputSurfaceProviderWebview {
 public:
  OutputSurfaceProviderWebview();
  ~OutputSurfaceProviderWebview();

  std::unique_ptr<viz::OutputSurface> CreateOutputSurface();

  const viz::RendererSettings& renderer_settings() const {
    return renderer_settings_;
  }
  scoped_refptr<AwGLSurface> gl_surface() const { return gl_surface_; }
  scoped_refptr<gpu::SharedContextState> shared_context_state() const {
    return shared_context_state_;
  }
  bool enable_shared_image() const { return enable_shared_image_; }

 private:
  void InitializeContext();

  // The member variables are effectively const after constructor, so it's safe
  // to call accessors on different threads
  viz::RendererSettings renderer_settings_;
  scoped_refptr<AwGLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  bool enable_shared_image_;
  bool enable_vulkan_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_OUTPUT_SURFACE_PROVIDER_WEBVIEW_H_
