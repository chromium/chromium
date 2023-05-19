// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/3pcd_heuristics/opener_heuristic_tab_helper.h"

#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/3pcd_heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/schemeful_site.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;

namespace {

// We don't need to protect this with a lock since it's only set while
// single-threaded in tests.
base::Clock* g_clock = nullptr;

base::Clock* GetClock() {
  return g_clock ? g_clock : base::DefaultClock::GetInstance();
}

}  // namespace

OpenerHeuristicTabHelper::OpenerHeuristicTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OpenerHeuristicTabHelper>(*web_contents) {}

OpenerHeuristicTabHelper::~OpenerHeuristicTabHelper() = default;

/* static */
base::Clock* OpenerHeuristicTabHelper::SetClockForTesting(base::Clock* clock) {
  return std::exchange(g_clock, clock);
}

void OpenerHeuristicTabHelper::InitPopup(const GURL& url) {
  popup_observer_ = std::make_unique<PopupObserver>(web_contents(), url);

  DIPSService* dips = DIPSService::Get(web_contents()->GetBrowserContext());
  if (!dips) {
    // If DIPS is disabled, we can't look up past interaction.
    // TODO(rtarpine): consider falling back to SiteEngagementService.
    return;
  }

  dips->storage()
      ->AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&OpenerHeuristicTabHelper::GotPopupDipsState,
                           weak_factory_.GetWeakPtr()));
}

void OpenerHeuristicTabHelper::GotPopupDipsState(const DIPSState& state) {
  if (!state.user_interaction_times().has_value()) {
    // No previous interaction.
    return;
  }

  popup_observer_->SetPastInteractionTime(
      state.user_interaction_times().value().second);
}

void OpenerHeuristicTabHelper::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (disposition != WindowOpenDisposition::NEW_POPUP) {
    // Ignore if not a popup.
    return;
  }

  if (!new_contents->HasOpener()) {
    // Ignore if popup doesn't have opener access.
    return;
  }

  // Create an OpenerHeuristicTabHelper for the popup.
  //
  // Note: TabHelpers::AttachTabHelpers() creates OpenerHeuristicTabHelper, but
  // on Android that can happen after DidOpenRequestedURL() is called (on other
  // platforms it seems to happen first). So create it now if it doesn't already
  // exist.
  OpenerHeuristicTabHelper::CreateForWebContents(new_contents);
  OpenerHeuristicTabHelper::FromWebContents(new_contents)->InitPopup(url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OpenerHeuristicTabHelper);

OpenerHeuristicTabHelper::PopupObserver::PopupObserver(
    WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents), initial_url_(url) {}

OpenerHeuristicTabHelper::PopupObserver::~PopupObserver() = default;

void OpenerHeuristicTabHelper::PopupObserver::SetPastInteractionTime(
    base::Time time) {
  CHECK(!time_since_interaction_.has_value())
      << "SetPastInteractionTime() called more than once";
  // Technically we should use the time when the pop-up first opened. But since
  // we only report this metric at hourly granularity, it shouldn't matter.
  time_since_interaction_ = GetClock()->Now() - time;

  // TODO(rtarpine): consider ignoring interactions that are too old. (This
  // shouldn't happen since DIPS already discards old timestamps.)

  EmitPastInteractionIfReady();
}

void OpenerHeuristicTabHelper::PopupObserver::EmitPastInteractionIfReady() {
  if (!time_since_interaction_.has_value() || !initial_source_id_.has_value()) {
    // Not enough information to emit event yet.
    return;
  }

  ukm::builders::OpenerHeuristic_PopupPastInteraction(
      initial_source_id_.value())
      .SetHoursSinceLastInteraction(
          BucketizeHoursSinceLastInteraction(time_since_interaction_.value()))
      .Record(ukm::UkmRecorder::Get());
}

void OpenerHeuristicTabHelper::PopupObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  url_index_ += navigation_handle->GetRedirectChain().size();

  if (initial_source_id_.has_value()) {
    // Only get the source id and time for the first commit. Ignore the rest.
    return;
  }

  commit_time_ = GetClock()->Now();

  if (navigation_handle->GetRedirectChain().size() > 1) {
    // Get a source id for the URL the popup was originally opened with,
    // even though the user was redirected elsewhere.
    initial_source_id_ = GetInitialRedirectSourceId(navigation_handle);
  } else {
    // No redirect happened, get the source id for the committed page.
    initial_source_id_ = navigation_handle->GetNextPageUkmSourceId();
  }

  EmitPastInteractionIfReady();
}

void OpenerHeuristicTabHelper::PopupObserver::FrameReceivedUserActivation(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (interaction_reported_) {
    // Only report the first interaction.
    return;
  }

  if (!commit_time_.has_value()) {
    // Not sure if this can happen. What happens if the user clicks before the
    // popup loads its initial URL?
    return;
  }

  auto time_since_committed = GetClock()->Now() - *commit_time_;
  ukm::builders::OpenerHeuristic_PopupInteraction(
      render_frame_host->GetPageUkmSourceId())
      .SetSecondsSinceCommitted(
          BucketizeSecondsSinceCommitted(time_since_committed))
      .SetUrlIndex(url_index_)
      .Record(ukm::UkmRecorder::Get());

  interaction_reported_ = true;
}
