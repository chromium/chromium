// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_

#include "base/macros.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"

namespace content {
class RenderFrameHost;
}

namespace dom_distiller {

namespace android {

class DistillerUIHandleAndroid : public DistillerUIHandle {
 public:
  DistillerUIHandleAndroid() {}
  ~DistillerUIHandleAndroid() override {}

  void set_render_frame_host(content::RenderFrameHost* host) {
    render_frame_host_ = host;
  }
  void OpenSettings() override;

 private:
  content::RenderFrameHost* render_frame_host_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(DistillerUIHandleAndroid);
};

}  // namespace android

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_
