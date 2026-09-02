// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

namespace glic {

namespace {

// LINT.IfChange(WebViewAutoPlayProgress)
enum class WebViewAutoPlayProgress {
  kWebContentsObserverRegistered = 0,
  kAutoPlayGrantedForPrimaryRFH = 1,
  kAutoPlayGrantedForOtherRFH = 2,
  kMaxValue = kAutoPlayGrantedForOtherRFH,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:WebViewAutoPlayProgress)

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicGuestObserver);

// static
void GlicGuestObserver::CreateForWebContents(
    content::WebContents& web_contents,
    GlicWebContentsManager& contents_manager) {
  if (FromWebContents(&web_contents)) {
    return;
  }
  web_contents.SetUserData(
      UserDataKey(),
      base::WrapUnique(new GlicGuestObserver(web_contents, contents_manager)));
}

GlicGuestObserver::GlicGuestObserver(content::WebContents& web_contents,
                                     GlicWebContentsManager& contents_manager)
    : content::WebContentsObserver(&web_contents),
      content::WebContentsUserData<GlicGuestObserver>(web_contents),
      contents_manager_(contents_manager) {}

GlicGuestObserver::~GlicGuestObserver() = default;

void GlicGuestObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MaybeEnableMojoJsBindings(render_frame_host);
}

void GlicGuestObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  GrantAutoplayPermissions(navigation_handle);
  MaybeEnableMojoJsBindings(navigation_handle);
}

void GlicGuestObserver::GrantAutoplayPermissions(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->AddAutoplayFlags(GetGuestOrigin(),
                           blink::mojom::kAutoplayFlagForceAllow);
  DVLOG(1) << "Granted Glic AutoPlay for origin=\"" << GetGuestOrigin()
           << "\" at "
           << (navigation_handle->IsInPrimaryMainFrame() ? "main " : "")
           << "RFH with url=\"" << navigation_handle->GetURL() << "\"";
  base::UmaHistogramEnumeration(
      "Glic.Host.WebView.AutoPlay",
      navigation_handle->IsInPrimaryMainFrame()
          ? WebViewAutoPlayProgress::kAutoPlayGrantedForPrimaryRFH
          : WebViewAutoPlayProgress::kAutoPlayGrantedForOtherRFH);
}

void GlicGuestObserver::MaybeEnableMojoJsBindings(
    content::RenderFrameHost* render_frame_host) {
  if (IsGlicGuest(web_contents())) {
    render_frame_host->EnableMojoJsBindings(/*features=*/nullptr);
  }
}

void GlicGuestObserver::MaybeEnableMojoJsBindings(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  // Enable MojoJS bindings if the pending navigation is targeting an allowed
  // origin so Blink can initialize the Mojo context during document load.
  // The frame's committed origin is checked in `BindGlicWebClientHandler()`
  // when the page attempts to bind the pipe.
  if (IsOriginAllowedGlicApi(
          url::Origin::Create(navigation_handle->GetURL()))) {
    navigation_handle->GetRenderFrameHost()->EnableMojoJsBindings(
        /*features=*/nullptr);
  }
}

}  // namespace glic
