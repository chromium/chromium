// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_safe_browsing_error_page_controller_delegate_impl.h"

#include "content/public/renderer/render_frame.h"

namespace android_webview {

AwSafeBrowsingErrorPageControllerDelegateImpl::
    AwSafeBrowsingErrorPageControllerDelegateImpl(
        content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<
          AwSafeBrowsingErrorPageControllerDelegateImpl>(render_frame) {}

AwSafeBrowsingErrorPageControllerDelegateImpl::
    ~AwSafeBrowsingErrorPageControllerDelegateImpl() = default;

void AwSafeBrowsingErrorPageControllerDelegateImpl::PrepareForErrorPage() {
  pending_error_ = true;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::OnDestruct() {
  delete this;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  committed_error_ = pending_error_;
  pending_error_ = false;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::DidFinishLoad() {
  if (committed_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame());
  }
}

}  // namespace android_webview
