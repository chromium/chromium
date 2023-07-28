// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/3pcd/heuristics/opener_heuristic_tab_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/3pcd/heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/3pcd/heuristics/opener_heuristic_utils.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
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

void OpenerHeuristicTabHelper::InitPopup(
    const GURL& popup_url,
    base::WeakPtr<OpenerHeuristicTabHelper> opener) {
  popup_observer_ =
      std::make_unique<PopupObserver>(web_contents(), popup_url, opener);

  DIPSService* dips = DIPSService::Get(web_contents()->GetBrowserContext());
  if (!dips) {
    // If DIPS is disabled, we can't look up past interaction.
    // TODO(rtarpine): consider falling back to SiteEngagementService.
    return;
  }

  dips->storage()
      ->AsyncCall(&DIPSStorage::Read)
      .WithArgs(popup_url)
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

void OpenerHeuristicTabHelper::PrimaryPageChanged(content::Page& page) {
  page_id_++;
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
  if (!source_render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (source_render_frame_host != web_contents()->GetPrimaryMainFrame()) {
    // Not sure exactly when this happens, but it seems to involve devtools.
    // Cf. crbug.com/1448789
    return;
  }

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
  OpenerHeuristicTabHelper::FromWebContents(new_contents)
      ->InitPopup(url, weak_factory_.GetWeakPtr());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OpenerHeuristicTabHelper);

OpenerHeuristicTabHelper::PopupObserver::PopupObserver(
    WebContents* web_contents,
    const GURL& initial_url,
    base::WeakPtr<OpenerHeuristicTabHelper> opener)
    : content::WebContentsObserver(web_contents),
      popup_id_(static_cast<int32_t>(base::RandUint64())),
      initial_url_(initial_url),
      opener_(opener),
      opener_page_id_(opener->page_id()),
      opener_source_id_(
          opener->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()) {
}

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

  auto has_iframe = GetOpenerHasSameSiteIframe(initial_url_);
  ukm::builders::OpenerHeuristic_PopupPastInteraction(
      initial_source_id_.value())
      .SetHoursSinceLastInteraction(Bucketize3PCDHeuristicTimeDelta(
          time_since_interaction_.value(), base::Days(30),
          base::BindRepeating(&base::TimeDelta::InHours)
              .Then(base::BindRepeating([](int64_t t) { return t; }))))
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupId(popup_id_)
      .Record(ukm::UkmRecorder::Get());

  EmitTopLevel(has_iframe);
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
  auto has_iframe =
      GetOpenerHasSameSiteIframe(render_frame_host->GetLastCommittedURL());
  ukm::builders::OpenerHeuristic_PopupInteraction(
      render_frame_host->GetPageUkmSourceId())
      .SetSecondsSinceCommitted(Bucketize3PCDHeuristicTimeDelta(
          time_since_committed, base::Minutes(3),
          base::BindRepeating(&base::TimeDelta::InSeconds)))
      .SetUrlIndex(url_index_)
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupId(popup_id_)
      .Record(ukm::UkmRecorder::Get());

  interaction_reported_ = true;

  EmitTopLevel(has_iframe);
}

void OpenerHeuristicTabHelper::PopupObserver::EmitTopLevel(
    OptionalBool has_iframe) {
  if (toplevel_reported_) {
    return;
  }

  ukm::builders::OpenerHeuristic_TopLevel(opener_source_id_)
      .SetHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupProvider(static_cast<int64_t>(GetPopupProvider(initial_url_)))
      .SetPopupId(popup_id_)
      .Record(ukm::UkmRecorder::Get());

  toplevel_reported_ = true;
}

OptionalBool
OpenerHeuristicTabHelper::PopupObserver::GetOpenerHasSameSiteIframe(
    const GURL& popup_url) {
  if (opener_ && opener_->page_id() == opener_page_id_) {
    return ToOptionalBool(
        HasSameSiteIframe(opener_->web_contents(), popup_url));
  }

  return OptionalBool::kUnknown;
}
