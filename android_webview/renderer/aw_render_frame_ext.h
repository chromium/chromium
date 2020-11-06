// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_

#include "android_webview/common/mojom/frame.mojom.h"
#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

namespace blink {
class WebFrameWidget;
class WebView;
}

namespace android_webview {

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderFrameExt : public content::RenderFrameObserver,
                         mojom::LocalMainFrame {
 public:
  explicit AwRenderFrameExt(content::RenderFrame* render_frame);

  static AwRenderFrameExt* FromRenderFrame(content::RenderFrame* render_frame);

 private:
  ~AwRenderFrameExt() override;

  // RenderFrameObserver:
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  bool OnMessageReceived(const IPC::Message& message) override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void OnDestruct() override;

  void OnDocumentHasImagesRequest(uint32_t id);
  void OnDoHitTest(const gfx::PointF& touch_center,
                   const gfx::SizeF& touch_area);

  void OnSetTextZoomFactor(float zoom_factor);

  void OnResetScrollAndScaleState();

  void OnSetInitialPageScale(double page_scale_factor);

  void OnSmoothScroll(int target_x, int target_y, base::TimeDelta duration);

  // mojom::LocalMainFrame overrides:
  void SetBackgroundColor(SkColor c) override;

  void BindLocalMainFrame(
      mojo::PendingAssociatedReceiver<mojom::LocalMainFrame> pending_receiver);

  blink::WebView* GetWebView();
  blink::WebFrameWidget* GetWebFrameWidget();

  url::Origin last_origin_;

  blink::AssociatedInterfaceRegistry registry_;
  mojo::AssociatedReceiver<mojom::LocalMainFrame> local_main_frame_receiver_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AwRenderFrameExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_
