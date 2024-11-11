// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_navigation_flow_detector.h"

#include "base/rand_util.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {
// Looks for a redirect to the current page that qualifies as a server-redirect
// exit from a suspected tracker flow (i.e., a single-hop server-side redirect)
// and returns it, if one exists. Returns nullptr otherwise.
const DIPSRedirectInfo* GetEntrypointExitServerRedirect(
    const DIPSRedirectContext& redirect_context) {
  size_t num_redirects = redirect_context.size();
  if (num_redirects == 0) {
    return nullptr;
  }

  size_t most_recent_redirect_index = num_redirects - 1;
  const DIPSRedirectInfo* most_recent_redirect =
      &redirect_context[most_recent_redirect_index];
  if (most_recent_redirect->redirect_type != DIPSRedirectType::kServer) {
    return nullptr;
  }

  bool is_single_hop_server_redirect =
      most_recent_redirect_index == 0 ||
      redirect_context[most_recent_redirect_index - 1].redirect_type !=
          DIPSRedirectType::kServer;
  if (!is_single_hop_server_redirect) {
    return nullptr;
  }

  return most_recent_redirect;
}

void EmitSuspectedTrackerFlowUkm(ukm::SourceId referrer_source_id,
                                 ukm::SourceId entrypoint_source_id,
                                 int32_t flow_id,
                                 DIPSRedirectType exit_redirect_type) {
  ukm::builders::DIPS_SuspectedTrackerFlowReferrer(referrer_source_id)
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());

  ukm::builders::DIPS_SuspectedTrackerFlowEntrypoint(entrypoint_source_id)
      .SetExitRedirectType(static_cast<int64_t>(exit_redirect_type))
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());
}
}  // namespace

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

PageVisitInfo& PageVisitInfo::operator=(PageVisitInfo&& other) = default;

bool PageVisitInfo::WasNavigationToPageClientRedirect() const {
  return was_navigation_to_page_renderer_initiated.has_value() &&
         *was_navigation_to_page_renderer_initiated &&
         was_navigation_to_page_user_initiated.has_value() &&
         !*was_navigation_to_page_user_initiated;
}

EntrypointInfo::EntrypointInfo(const DIPSRedirectInfo& server_redirect_info,
                               const dips::PageVisitInfo& exit_page_info)
    : site(server_redirect_info.site),
      source_id(server_redirect_info.url.source_id),
      had_triggering_storage_access(
          server_redirect_info.access_type == SiteDataAccessType::kWrite ||
          server_redirect_info.access_type == SiteDataAccessType::kReadWrite),
      was_referral_client_redirect(
          exit_page_info.WasNavigationToPageClientRedirect()) {}

EntrypointInfo::EntrypointInfo(
    const dips::PageVisitInfo& client_redirector_info)
    : site(client_redirector_info.site),
      source_id(client_redirector_info.source_id),
      had_triggering_storage_access(
          client_redirector_info.did_page_access_storage ||
          client_redirector_info.did_page_access_cookies),
      was_referral_client_redirect(
          client_redirector_info.WasNavigationToPageClientRedirect()) {}

}  // namespace dips

DipsNavigationFlowDetector::DipsNavigationFlowDetector(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DipsNavigationFlowDetector>(*web_contents),
      current_page_visit_info_(dips::PageVisitInfo()) {
  redirect_chain_observation_.Observe(
      RedirectChainDetector::FromWebContents(web_contents));
}

DipsNavigationFlowDetector::~DipsNavigationFlowDetector() = default;

void DipsNavigationFlowDetector::OnNavigationCommitted(
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

  two_pages_ago_visit_info_ = std::move(previous_page_visit_info_);
  previous_page_visit_info_ = std::move(current_page_visit_info_);
  current_page_visit_info_.emplace();

  current_page_visit_info_->url = current_page_url;
  current_page_visit_info_->site = GetSiteForDIPS(current_page_url);
  current_page_visit_info_->source_id = render_frame_host->GetPageUkmSourceId();
  current_page_visit_info_->was_navigation_to_page_renderer_initiated =
      navigation_handle->IsRendererInitiated();
  current_page_visit_info_->was_navigation_to_page_user_initiated =
      !navigation_handle->IsRendererInitiated() ||
      navigation_handle->HasUserGesture();
  if (navigation_cookie_access_url_ == current_page_url) {
    current_page_visit_info_->did_page_access_cookies = true;
  }
  navigation_cookie_access_url_ = std::nullopt;

  base::Time now = clock_->Now();
  if (!is_first_page_load_in_tab) {
    int64_t raw_visit_duration_ms =
        (now - last_page_change_time_).InMilliseconds();
    bucketized_previous_page_visit_duration_ =
        ukm::GetExponentialBucketMinForUserTiming(raw_visit_duration_ms);
  }
  last_page_change_time_ = now;

  MaybeEmitNavFlowNodeUkmForPreviousPage();

  int32_t flow_id = static_cast<int32_t>(base::RandUint64());
  const DIPSRedirectInfo* server_redirect_entrypoint_exit =
      GetEntrypointExitServerRedirect(
          redirect_chain_observation_.GetSource()->CommittedRedirectContext());
  if (server_redirect_entrypoint_exit != nullptr) {
    MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        server_redirect_entrypoint_exit, flow_id);
  } else {
    MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(flow_id);
    MaybeEmitInFlowInteraction(flow_id);
  }
}

void DipsNavigationFlowDetector::MaybeEmitNavFlowNodeUkmForPreviousPage() {
  if (!CanEmitNavFlowNodeUkmForPreviousPage()) {
    return;
  }

  ukm::builders::DIPS_NavigationFlowNode(previous_page_visit_info_->source_id)
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
      .SetVisitDurationMilliseconds(bucketized_previous_page_visit_duration_)
      .Record(ukm::UkmRecorder::Get());
}

bool DipsNavigationFlowDetector::CanEmitNavFlowNodeUkmForPreviousPage() const {
  bool page_is_in_series_of_three = two_pages_ago_visit_info_.has_value() &&
                                    !two_pages_ago_visit_info_->site.empty() &&
                                    previous_page_visit_info_.has_value() &&
                                    !previous_page_visit_info_->site.empty() &&
                                    current_page_visit_info_.has_value() &&
                                    !current_page_visit_info_->site.empty();
  if (!page_is_in_series_of_three) {
    return false;
  }

  bool page_has_valid_source_id =
      previous_page_visit_info_->source_id != ukm::kInvalidSourceId;
  bool site_had_triggering_storage_access =
      previous_page_visit_info_->did_page_access_cookies ||
      previous_page_visit_info_->did_page_access_storage;
  bool is_site_different_from_prior_page =
      previous_page_visit_info_->site != two_pages_ago_visit_info_->site;
  bool is_site_different_from_next_page =
      previous_page_visit_info_->site != current_page_visit_info_->site;

  return page_has_valid_source_id && site_had_triggering_storage_access &&
         is_site_different_from_prior_page && is_site_different_from_next_page;
}

void DipsNavigationFlowDetector::
    MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        const DIPSRedirectInfo* exit_info,
        int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(exit_info)) {
    return;
  }

  EmitSuspectedTrackerFlowUkm(previous_page_visit_info_->source_id,
                              exit_info->url.source_id, flow_id,
                              DIPSRedirectType::kServer);
}

bool DipsNavigationFlowDetector::
    CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        const DIPSRedirectInfo* exit_info) const {
  if (!previous_page_visit_info_.has_value() || exit_info == nullptr ||
      !current_page_visit_info_.has_value()) {
    return false;
  }

  dips::EntrypointInfo entrypoint_info_for_server_redirect_exit(
      *exit_info, *current_page_visit_info_);
  return CanEmitSuspectedTrackerFlowUkm(
      *previous_page_visit_info_, entrypoint_info_for_server_redirect_exit,
      *current_page_visit_info_);
}

void DipsNavigationFlowDetector::
    MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForClientRedirectExit()) {
    return;
  }

  EmitSuspectedTrackerFlowUkm(two_pages_ago_visit_info_->source_id,
                              previous_page_visit_info_->source_id, flow_id,
                              DIPSRedirectType::kClient);
}

bool DipsNavigationFlowDetector::
    CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() const {
  bool page_is_in_series_of_three = two_pages_ago_visit_info_.has_value() &&
                                    previous_page_visit_info_.has_value() &&
                                    current_page_visit_info_.has_value();
  if (!page_is_in_series_of_three) {
    return false;
  }

  std::optional<bool> is_exit_client_redirect =
      current_page_visit_info_->WasNavigationToPageClientRedirect();
  if (!is_exit_client_redirect.has_value() ||
      !is_exit_client_redirect.value()) {
    return false;
  }

  dips::EntrypointInfo entrypoint_info(previous_page_visit_info_.value());
  return CanEmitSuspectedTrackerFlowUkm(two_pages_ago_visit_info_.value(),
                                        entrypoint_info,
                                        current_page_visit_info_.value());
}

bool DipsNavigationFlowDetector::CanEmitSuspectedTrackerFlowUkm(
    const dips::PageVisitInfo& referrer_page_info,
    const dips::EntrypointInfo& entrypoint_info,
    const dips::PageVisitInfo& exit_page_info) const {
  bool referrer_has_valid_source_id =
      referrer_page_info.source_id != ukm::kInvalidSourceId;
  bool entrypoint_has_valid_source_id =
      entrypoint_info.source_id != ukm::kInvalidSourceId;
  bool is_entrypoint_site_different_from_referrer =
      entrypoint_info.site != referrer_page_info.site;
  bool is_entrypoint_site_different_from_exit_page =
      entrypoint_info.site != exit_page_info.site;

  return referrer_has_valid_source_id && entrypoint_has_valid_source_id &&
         is_entrypoint_site_different_from_referrer &&
         is_entrypoint_site_different_from_exit_page &&
         entrypoint_info.had_triggering_storage_access &&
         entrypoint_info.was_referral_client_redirect;
}

void DipsNavigationFlowDetector::MaybeEmitInFlowInteraction(int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() ||
      !previous_page_visit_info_->did_page_receive_user_activation) {
    return;
  }

  ukm::builders::DIPS_TrustIndicator_InFlowInteraction(
      previous_page_visit_info_->source_id)
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());
}

void DipsNavigationFlowDetector::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Ignore notifications for prerenders, fenced frames, etc., and for blocked
  // access attempts.
  if (!dips::IsOrWasInPrimaryPage(render_frame_host) ||
      details.blocked_by_policy) {
    return;
  }
  // Attribute accesses by iframes to the first-party page they're embedded in.
  const std::optional<GURL> first_party_url =
      GetFirstPartyURL(render_frame_host);
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
  if (details.type != CookieOperation::kChange) {
    return;
  }

  if (details.url == current_page_visit_info_->url) {
    current_page_visit_info_->did_page_access_cookies = true;
  } else {
    // This notification might be for an in-progress navigation, so we should
    // remember this notification until the next navigation finishes.
    navigation_cookie_access_url_ = details.url;
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

void DipsNavigationFlowDetector::WebContentsDestroyed() {
  redirect_chain_observation_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DipsNavigationFlowDetector);
