// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"

#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

MediaFeedsContentsObserver::MediaFeedsContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // The cookie observer cannot be created at initialization of
  // MediaFeedsService because the network service is not ready and therefore we
  // should create it when we get a web contents.
  if (auto* service = GetService())
    service->EnsureCookieObserver();
}

MediaFeedsContentsObserver::~MediaFeedsContentsObserver() = default;

void MediaFeedsContentsObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() || handle->IsSameDocument())
    return;

  render_frame_.reset();

  auto new_origin = url::Origin::Create(web_contents()->GetLastCommittedURL());
  if (last_origin_ == new_origin)
    return;

  ResetFeed();
  last_origin_ = new_origin;
}

void MediaFeedsContentsObserver::WebContentsDestroyed() {
  ResetFeed();
}

void MediaFeedsContentsObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent() || !GetService())
    return;

  // We should only discover Media Feeds on secure origins.
  if (!validated_url.SchemeIsCryptographic()) {
    if (test_closure_)
      std::move(test_closure_).Run();
    return;
  }

  // Clear the old binding for the old frame.
  render_frame_.reset();

  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &render_frame_);

  // Unretained is safe here because MediaFeedsContentsObserver owns the mojo
  // remote.
  render_frame_->GetMediaFeedURL(base::BindOnce(
      &MediaFeedsContentsObserver::DidFindMediaFeed, base::Unretained(this),
      render_frame_host->GetLastCommittedOrigin()));
}

void MediaFeedsContentsObserver::DidFindMediaFeed(
    const url::Origin& origin,
    const base::Optional<GURL>& url) {
  auto* service = GetService();
  if (!service)
    return;

  // The feed should be the same origin as the original page that had the feed
  // on it.
  if (url) {
    if (!origin.IsSameOriginWith(url::Origin::Create(*url))) {
      mojo::ReportBadMessage(
          "GetMediaFeedURL. The URL should be the same origin has the page.");
      return;
    }

    base::Optional<GURL> favicon;
    for (auto& found : web_contents()->GetFaviconURLs()) {
      if (found->icon_type == blink::mojom::FaviconIconType::kFavicon) {
        favicon = found->icon_url;
      }
    }

    CHECK(url->SchemeIsCryptographic());
    service->DiscoverMediaFeed(*url, favicon);

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    if (!ukm_recorder)
      return;

    ukm::builders::Media_Feed_Discover(
        web_contents()->GetMainFrame()->GetPageUkmSourceId())
        .SetHasMediaFeed(true)
        .Record(ukm_recorder);
  }

  if (test_closure_)
    std::move(test_closure_).Run();
}

media_feeds::MediaFeedsService* MediaFeedsContentsObserver::GetService() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (profile->IsOffTheRecord())
    return nullptr;

  return media_feeds::MediaFeedsServiceFactory::GetForProfile(profile);
}

void MediaFeedsContentsObserver::ResetFeed() {
  if (!last_origin_.has_value())
    return;

  if (auto* service = GetService()) {
    service->ResetMediaFeed(*last_origin_,
                            media_feeds::mojom::ResetReason::kVisit);
  }

  last_origin_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaFeedsContentsObserver)
