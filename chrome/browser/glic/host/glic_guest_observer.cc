// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/glic/host/glic_theme_util.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "ui/gfx/geometry/size.h"

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

BASE_FEATURE(kGlicUpdateHiddenScreenInfo,
             FEATURE_ENABLED_BY_DEFAULT_NON_ANDROID);

}  // namespace

GlicGuestObserver::Metrics::Metrics() = default;
GlicGuestObserver::Metrics::~Metrics() = default;

void GlicGuestObserver::Metrics::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Profile* profile =
      navigation_handle && navigation_handle->GetWebContents()
          ? Profile::FromBrowserContext(
                navigation_handle->GetWebContents()->GetBrowserContext())
          : nullptr;
  if (!GlicKeyedService::IsNoWebviewEnabled(profile)) {
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

void GlicGuestObserver::Metrics::DocumentOnLoadCompletedInPrimaryMainFrame(
    content::WebContents* web_contents) {
  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  if (!GlicKeyedService::IsNoWebviewEnabled(profile)) {
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
  url::Origin origin =
      navigation_handle->GetOriginToCommit().value_or(url::Origin());
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
      contents_manager_(contents_manager) {
  MaybeUpdateTopLevelScreenInfo();
}

GlicGuestObserver::~GlicGuestObserver() = default;

void GlicGuestObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MaybeSetBackgroundColor(render_frame_host);
}
void GlicGuestObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  GrantAutoplayPermissions(navigation_handle);
  MaybeEnableMojoJsBindings(navigation_handle);
  if (navigation_handle->IsInPrimaryMainFrame()) {
    MaybeUpdateTopLevelScreenInfo();
  }
}

void GlicGuestObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  metrics_.DidFinishNavigation(navigation_handle);
}

void GlicGuestObserver::DocumentOnLoadCompletedInPrimaryMainFrame() {
  metrics_.DocumentOnLoadCompletedInPrimaryMainFrame(web_contents());
}

void GlicGuestObserver::MaybeEnableMojoJsBindings(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  // Enable MojoJS bindings if the pending navigation is committing an allowed
  // origin so Blink can initialize the Mojo context during document load.
  // The frame's committed origin is checked in `BindGlicWebClientHandler()`
  // when the page attempts to bind the pipe.
  url::Origin origin =
      navigation_handle->GetOriginToCommit().value_or(url::Origin());
  if (IsOriginAllowedGlicApi(
          origin, navigation_handle->GetWebContents()->GetBrowserContext())) {
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

void GlicGuestObserver::MaybeUpdateTopLevelScreenInfo() {
  if (!web_contents() ||
      base::FeatureList::IsEnabled(
          features::kPopulateUnattachedAuraScreenInfo) ||
      !base::FeatureList::IsEnabled(kGlicUpdateHiddenScreenInfo)) {
    return;
  }
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents());
  if (top && top->GetVisibility() == content::Visibility::HIDDEN) {
    // TODO(b/573268718): When kPopulateUnattachedAuraScreenInfo is disabled,
    // RenderWidgetHostViewAura::InitAsChild leaves screen_infos_ at 0x0 when a
    // WebContents is created unattached and hidden, causing the guest's
    // window.screen.height to be 0 during warmup. Pulsing
    // IncrementCapturerCount(stay_hidden=true) triggers
    // ShowWithVisibility(kHiddenButPainting) -> UpdateScreenInfo() ->
    // Hide() while keeping the WebContents hidden.
    std::ignore = top->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                              /*stay_awake=*/false,
                                              /*is_activity=*/false);
  }
}

}  // namespace glic
