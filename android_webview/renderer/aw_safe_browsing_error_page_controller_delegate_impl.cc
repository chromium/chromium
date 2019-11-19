// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_safe_browsing_error_page_controller_delegate_impl.h"

#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

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

mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
AwSafeBrowsingErrorPageControllerDelegateImpl::GetInterface() {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&interface);
  return interface;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::OnDestruct() {
  delete this;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  weak_controller_delegate_factory_.InvalidateWeakPtrs();
  committed_error_ = pending_error_;
  pending_error_ = false;
}

void AwSafeBrowsingErrorPageControllerDelegateImpl::DidFinishLoad() {
  if (committed_error_) {
    security_interstitials::SecurityInterstitialPageController::Install(
        render_frame(), weak_controller_delegate_factory_.GetWeakPtr());
  }
}

}  // namespace android_webview
