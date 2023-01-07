// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_

#include "base/timer/timer.h"
#include "third_party/blink/public/web/web_view_observer.h"
#include "ui/gfx/geometry/size.h"

namespace android_webview {

// NOTE: We should not add more things to RenderView and related classes.
//       RenderView is deprecated in content, since it is not compatible
//       with site isolation/out of process iframes.

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderViewExt : public blink::WebViewObserver {
 public:
  AwRenderViewExt(const AwRenderViewExt&) = delete;
  AwRenderViewExt& operator=(const AwRenderViewExt&) = delete;

  static void WebViewCreated(blink::WebView* web_view,
                             bool created_by_renderer);

  static AwRenderViewExt* FromWebView(blink::WebView* web_view);

  bool created_by_renderer() { return created_by_renderer_; }

 private:
  AwRenderViewExt(blink::WebView* web_view, bool created_by_renderer);
  ~AwRenderViewExt() override;

  // blink::WebViewObserver overrides.
  void DidCommitCompositorFrame() override;
  void DidUpdateMainFrameLayout() override;
  void OnDestruct() override;

  void UpdateContentsSize();

  gfx::Size last_sent_contents_size_;

  // Whether the contents size may have changed and |UpdateContentsSize| needs
  // to be called.
  bool needs_contents_size_update_ = true;

  bool created_by_renderer_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
