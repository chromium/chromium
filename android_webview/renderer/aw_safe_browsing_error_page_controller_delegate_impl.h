// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_

#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace android_webview {

class AwSafeBrowsingErrorPageControllerDelegateImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          AwSafeBrowsingErrorPageControllerDelegateImpl> {
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

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;

 private:
  // Whether there is an error page pending to be committed.
  bool pending_error_ = false;

  // Whether the committed page is an error page.
  bool committed_error_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_SAFE_BROWSING_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
