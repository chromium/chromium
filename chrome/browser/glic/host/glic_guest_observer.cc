// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/host/glic_theme_util.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
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

GlicGuestObserver::Metrics::Metrics() = default;
GlicGuestObserver::Metrics::~Metrics() = default;

void GlicGuestObserver::Metrics::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!features::IsGlicNoWebviewEnabled()) {
    return;
  }
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument() &&
      !navigation_handle->IsErrorPage() && navigation_commit_time_.is_null()) {
    navigation_commit_time_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("Glic.Contents.NavigationCommitTime",
                            navigation_commit_time_ - creation_time_);
  }
}

void GlicGuestObserver::Metrics::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!features::IsGlicNoWebviewEnabled()) {
    return;
  }
  if (!navigation_commit_time_.is_null() && !has_recorded_load_complete_) {
    has_recorded_load_complete_ = true;
    base::UmaHistogramTimes("Glic.Contents.LoadCompleteTime",
                            base::TimeTicks::Now() - navigation_commit_time_);
  }
}

void GrantAutoplayPermissions(content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  url::Origin origin = url::Origin::Create(navigation_handle->GetURL());
  if (!IsGuestOriginAllowed(
          origin, navigation_handle->GetWebContents()->GetBrowserContext())) {
    return;
  }
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->AddAutoplayFlags(origin, blink::mojom::kAutoplayFlagForceAllow);
  DVLOG(1) << "Granted Glic AutoPlay for origin=\"" << origin
           << "\" at primary main RFH with url=\""
           << navigation_handle->GetURL() << "\"";
  base::UmaHistogramEnumeration(
      "Glic.Host.WebView.AutoPlay",
      WebViewAutoPlayProgress::kAutoPlayGrantedForPrimaryRFH);
}

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
  MaybeSetBackgroundColor(render_frame_host);
}
void GlicGuestObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  GrantAutoplayPermissions(navigation_handle);
  MaybeEnableMojoJsBindings(navigation_handle);
}

void GlicGuestObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  metrics_.DidFinishNavigation(navigation_handle);
}

void GlicGuestObserver::DocumentOnLoadCompletedInPrimaryMainFrame() {
  metrics_.DocumentOnLoadCompletedInPrimaryMainFrame();
}

void GlicGuestObserver::MaybeEnableMojoJsBindings(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  // Enable MojoJS bindings if the pending navigation is targeting an allowed
  // origin so Blink can initialize the Mojo context during document load.
  // The frame's committed origin is checked in `BindGlicWebClientHandler()`
  // when the page attempts to bind the pipe.
  if (IsOriginAllowedGlicApi(
          url::Origin::Create(navigation_handle->GetURL()),
          navigation_handle->GetWebContents()->GetBrowserContext())) {
    navigation_handle->GetRenderFrameHost()->EnableMojoJsBindings(
        /*features=*/nullptr);
  }
}

void GlicGuestObserver::MaybeSetBackgroundColor(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParentOrOuterDocument() ||
      !render_frame_host->GetView() || !web_contents()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_frame_host->GetView()->SetBackgroundColor(
      GetGlicBackgroundColor(profile, web_contents()->GetColorProvider()));
}

}  // namespace glic
