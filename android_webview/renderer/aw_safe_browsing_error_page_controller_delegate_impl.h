// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace android_webview {

class AwSafeBrowsingErrorPageControllerDelegateImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          AwSafeBrowsingErrorPageControllerDelegateImpl>,
      public security_interstitials::SecurityInterstitialPageController::
          Delegate {
 public:
  explicit AwSafeBrowsingErrorPageControllerDelegateImpl(
      content::RenderFrame* render_frame);

  // Disallow copy and assign
  AwSafeBrowsingErrorPageControllerDelegateImpl(
      const AwSafeBrowsingErrorPageControllerDelegateImpl&) = delete;
  AwSafeBrowsingErrorPageControllerDelegateImpl& operator=(
      const AwSafeBrowsingErrorPageControllerDelegateImpl&) = delete;

  ~AwSafeBrowsingErrorPageControllerDelegateImpl() override;

  // Notifies us that a navigation error has occurred and will be committed
  void PrepareForErrorPage();

  // security_interstitials::SecurityInterstitialPageController::Delegate:
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
  GetInterface() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidFinishLoad() override;

 private:
  // Whether there is an error page pending to be committed.
  bool pending_error_ = false;

  // Whether the committed page is an error page.
  bool committed_error_ = false;

  base::WeakPtrFactory<AwSafeBrowsingErrorPageControllerDelegateImpl>
      weak_controller_delegate_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
