// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace android_webview {

class AwRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit AwRenderFrameObserver(content::RenderFrame* render_frame);
  ~AwRenderFrameObserver() override;

 private:
  // RenderFrameObserver implementation.
  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override;
  void OnDestruct() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_
