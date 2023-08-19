// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// BrowserRenderer and its delegates talk to VrShell through this interface.
class GlBrowserInterface {
 public:
  virtual ~GlBrowserInterface() {}

  virtual void ForceExitVr() = 0;
  virtual void GvrDelegateReady() = 0;
  // XRSessionPtr is optional, if null, the request failed.
  virtual void SendRequestPresentReply(device::mojom::XRSessionPtr) = 0;
  virtual void ToggleCardboardGamepad(bool enabled) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_
