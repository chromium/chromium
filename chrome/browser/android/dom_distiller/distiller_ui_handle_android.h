// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"

namespace content {
class RenderFrameHost;
}

namespace dom_distiller {

namespace android {

class DistillerUIHandleAndroid : public DistillerUIHandle {
 public:
  DistillerUIHandleAndroid() {}

  DistillerUIHandleAndroid(const DistillerUIHandleAndroid&) = delete;
  DistillerUIHandleAndroid& operator=(const DistillerUIHandleAndroid&) = delete;

  ~DistillerUIHandleAndroid() override {}

  void set_render_frame_host(content::RenderFrameHost* host) {
    render_frame_host_ = host;
  }
  void OpenSettings() override;

 private:
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;
};

}  // namespace android

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_ANDROID_DOM_DISTILLER_DISTILLER_UI_HANDLE_ANDROID_H_
