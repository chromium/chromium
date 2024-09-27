// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_navigation_flow_detector.h"

#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace dips {

PageVisitInfo::PageVisitInfo() {
  site = "";
  source_id = ukm::kInvalidSourceId;
  did_page_access_cookies = false;
  did_page_access_storage = false;
  did_page_receive_user_activation = false;
  did_page_have_successful_waa = false;
  was_navigation_to_page_user_initiated = std::nullopt;
  was_navigation_to_page_renderer_initiated = std::nullopt;
}

PageVisitInfo::PageVisitInfo(PageVisitInfo&& other) = default;

}  // namespace dips

DipsNavigationFlowDetector::DipsNavigationFlowDetector(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DipsNavigationFlowDetector>(*web_contents),
      current_page_visit_info_(dips::PageVisitInfo()) {}

DipsNavigationFlowDetector::~DipsNavigationFlowDetector() = default;

void DipsNavigationFlowDetector::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  bool primary_page_changed = navigation_handle->IsInPrimaryMainFrame() &&
                              !navigation_handle->IsSameDocument() &&
                              navigation_handle->HasCommitted();
  if (!primary_page_changed) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetWebContents()->GetPrimaryMainFrame();

  GURL current_page_url = render_frame_host->GetLastCommittedURL();
  if (current_page_url == url::kAboutBlankURL) {
    return;
  }

  bool is_first_page_load_in_tab = current_page_visit_info_->site.empty();
  if (!is_first_page_load_in_tab) {
    if (previous_page_visit_info_) {
      two_pages_ago_visit_info_.emplace(std::move(*previous_page_visit_info_));
    }
    if (current_page_visit_info_) {
      previous_page_visit_info_.emplace(std::move(*current_page_visit_info_));
    }
    current_page_visit_info_.emplace(dips::PageVisitInfo());
  }

  current_page_visit_info_->site = GetSiteForDIPS(current_page_url);
  current_page_visit_info_->source_id = render_frame_host->GetPageUkmSourceId();
  current_page_visit_info_->was_navigation_to_page_renderer_initiated =
      navigation_handle->IsRendererInitiated();
  current_page_visit_info_->was_navigation_to_page_user_initiated =
      !navigation_handle->IsRendererInitiated() ||
      navigation_handle->HasUserGesture();

  base::Time now = clock_->Now();
  if (!is_first_page_load_in_tab) {
    int64_t raw_visit_duration_ms =
        (now - last_page_change_time_).InMilliseconds();
    bucketized_previous_page_visit_duration_ =
        ukm::GetExponentialBucketMinForUserTiming(raw_visit_duration_ms);
  }
  last_page_change_time_ = now;

  MaybeEmitUkmForPreviousPage();
}

void DipsNavigationFlowDetector::MaybeEmitUkmForPreviousPage() {
  if (!CanEmitUkmForPreviousPage()) {
    return;
  }

  ukm::builders::DIPS_NavigationFlowNode builder(
      previous_page_visit_info_->source_id);
  builder
      .SetWerePreviousAndNextSiteSame(two_pages_ago_visit_info_->site ==
                                      current_page_visit_info_->site)
      .SetDidHaveUserActivation(
          previous_page_visit_info_->did_page_receive_user_activation)
      .SetDidHaveSuccessfulWAA(
          previous_page_visit_info_->did_page_have_successful_waa)
      .SetWereEntryAndExitRendererInitiated(
          *previous_page_visit_info_
               ->was_navigation_to_page_renderer_initiated &&
          *current_page_visit_info_->was_navigation_to_page_renderer_initiated)
      .SetWasEntryUserInitiated(
          *previous_page_visit_info_->was_navigation_to_page_user_initiated)
      .SetWasExitUserInitiated(
          *current_page_visit_info_->was_navigation_to_page_user_initiated)
      .SetVisitDurationMilliseconds(bucketized_previous_page_visit_duration_);
  builder.Record(ukm::UkmRecorder::Get());
}

void DipsNavigationFlowDetector::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Ignore notifications for prerenders, fenced frames, etc., and for blocked
  // access attempts.
  if (!IsInPrimaryPage(render_frame_host) || details.blocked_by_policy) {
    return;
  }
  // Attribute accesses by iframes to the first-party page they're embedded in.
  const std::optional<GURL> first_party_url =
      GetFirstPartyURL(render_frame_host);
  if (!first_party_url.has_value()) {
    return;
  }
  const std::string first_party_site = GetSiteForDIPS(first_party_url.value());
  // DIPS mitigations are only turned on when non-CHIPS 3PCs are blocked, so
  // mirror that behavior by ignoring non-CHIPS 3PC accesses.
  if (!HasCHIPS(details.cookie_access_result_list) &&
      !IsSameSiteForDIPS(first_party_url.value(), details.url)) {
    return;
  }
  // If the site we received the cookie access notification for is not the same
  // as the current site, that means that site has since been navigated away
  // from. In that case, we've already emitted UKM (or decided not to emit) for
  // that page, so ignore the notification.
  if (first_party_site != current_page_visit_info_->site) {
    return;
  }

  current_page_visit_info_->did_page_access_cookies = true;
}

void DipsNavigationFlowDetector::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  // Ignore notifications for prerenders, fenced frames, etc., and for blocked
  // access attempts.
  if (!IsInPrimaryPage(navigation_handle) || details.blocked_by_policy) {
    return;
  }

  // Treat cookie accesses from iframe navigations as content-initiated.
  if (IsInPrimaryPageIFrame(navigation_handle)) {
    const std::optional<GURL> first_party_url =
        GetFirstPartyURL(navigation_handle);
    if (!first_party_url.has_value()) {
      return;
    }

    // DIPS mitigations are only turned on when non-CHIPS 3PCs are blocked, so
    // mirror that behavior by ignoring non-CHIPS 3PC accesses.
    if (!HasCHIPS(details.cookie_access_result_list) &&
        !IsSameSiteForDIPS(first_party_url.value(), details.url)) {
      return;
    }

    current_page_visit_info_->did_page_access_cookies = true;
    return;
  }

  // For accesses in main frame navigations, only count writes, as the browser
  // sends cookies automatically and so sites have no control over whether they
  // read cookies or not.
  if (details.type == network::mojom::CookieAccessDetails_Type::kChange) {
    current_page_visit_info_->did_page_access_cookies = true;
  }
}

void DipsNavigationFlowDetector::NotifyStorageAccessed(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::StorageTypeAccessed storage_type,
    bool blocked) {
  if (!render_frame_host->IsInPrimaryMainFrame() || blocked) {
    return;
  }
  current_page_visit_info_->did_page_access_storage = true;
}

void DipsNavigationFlowDetector::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  current_page_visit_info_->did_page_receive_user_activation = true;
}

void DipsNavigationFlowDetector::WebAuthnAssertionRequestSucceeded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  current_page_visit_info_->did_page_have_successful_waa = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DipsNavigationFlowDetector);
