// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_BROWSER_RENDERER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_VR_BROWSER_RENDERER_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/vr_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gvr {
class GvrApi;
}

namespace base {
class WaitableEvent;
}

namespace vr {

class BrowserRenderer;
class UiFactory;
class VrGLThread;

class VR_EXPORT BrowserRendererFactory {
 public:
  struct VR_EXPORT Params {
    Params(gvr::GvrApi* gvr_api,
           const UiInitialState& ui_initial_state,
           bool reprojected_rendering,
           bool cardboard_gamepad,
           bool pause_content,
           bool low_density,
           base::WaitableEvent* gl_surface_created_event,
           base::OnceCallback<gfx::AcceleratedWidget()> surface_callback);
    ~Params();
    gvr::GvrApi* gvr_api;
    UiInitialState ui_initial_state;
    bool reprojected_rendering;
    bool cardboard_gamepad;
    bool pause_content;
    bool low_density;
    base::WaitableEvent* gl_surface_created_event;
    base::OnceCallback<gfx::AcceleratedWidget()> surface_callback;
  };

  static std::unique_ptr<BrowserRenderer> Create(
      VrGLThread* vr_gl_thread,
      UiFactory* ui_factory,
      std::unique_ptr<Params> params);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_BROWSER_RENDERER_FACTORY_H_
