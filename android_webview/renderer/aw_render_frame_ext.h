// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_

#include "android_webview/common/mojom/frame.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

namespace blink {
class WebFrameWidget;
class WebHitTestResult;
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

  AwRenderFrameExt(const AwRenderFrameExt&) = delete;
  AwRenderFrameExt& operator=(const AwRenderFrameExt&) = delete;

 private:
  ~AwRenderFrameExt() override;

  // RenderFrameObserver:
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  void FocusedElementChanged(const blink::WebElement& element) override;
  void DidCreateDocumentElement() override;
  void OnDestruct() override;

  // mojom::LocalMainFrame overrides:
  void SetInitialPageScale(double page_scale_factor) override;
  void SetTextZoomFactor(float zoom_factor) override;
  void HitTest(const gfx::PointF& touch_center,
               const gfx::SizeF& touch_area) override;
  void DocumentHasImage(DocumentHasImageCallback callback) override;
  void ResetScrollAndScaleState() override;
  void SmoothScroll(int32_t target_x,
                    int32_t target_y,
                    base::TimeDelta duration) override;

  void BindLocalMainFrame(
      mojo::PendingAssociatedReceiver<mojom::LocalMainFrame> pending_receiver);

  void HandleHitTestResult(const blink::WebHitTestResult& result);

  const mojo::AssociatedRemote<mojom::FrameHost>& GetFrameHost();

  blink::WebView* GetWebView();
  blink::WebFrameWidget* GetWebFrameWidget();

  url::Origin last_origin_;

  blink::AssociatedInterfaceRegistry registry_;
  mojo::AssociatedReceiver<mojom::LocalMainFrame> local_main_frame_receiver_{
      this};

  mojo::AssociatedRemote<mojom::FrameHost> frame_host_remote_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_
