// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/commit_limit_oom_recovery_tracker.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/chrome_result_codes.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

DEFINE_USER_DATA(CommitLimitOOMRecoveryTracker);

namespace {

void RecordReloadResult(CommitLimitOOMRecoveryTracker::ReloadResult result) {
  base::UmaHistogramEnumeration(
      "Stability.CommitLimitTerminatedTab.ReloadResult", result);
}

}  // namespace

CommitLimitOOMRecoveryTracker::CommitLimitOOMRecoveryTracker(
    tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this),
      discard_subscription_(tab.RegisterWillDiscardContents(
          base::BindRepeating(&CommitLimitOOMRecoveryTracker::OnTabDiscarded,
                              base::Unretained(this)))) {}

CommitLimitOOMRecoveryTracker::~CommitLimitOOMRecoveryTracker() = default;

// static
CommitLimitOOMRecoveryTracker* CommitLimitOOMRecoveryTracker::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void CommitLimitOOMRecoveryTracker::OnTabDiscarded(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

void CommitLimitOOMRecoveryTracker::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Normal termination (e.g. fast shutdown on tab/browser close) is not a
    // recovery failure and is treated as censored.
    TransitionTo(TrackingState::kIdle);
    return;
  }

  if (state_ != TrackingState::kIdle) {
    RecordReloadResult(IsTerminatedByCommitFailure()
                           ? ReloadResult::kFailedOOM
                           : ReloadResult::kFailedOther);
    TransitionTo(TrackingState::kIdle);
    return;
  }

  if (!IsTerminatedByCommitFailure()) {
    return;
  }

  const content::Visibility visibility = web_contents()->GetVisibility();
  TerminationVisibility term_visibility;
  switch (visibility) {
    case content::Visibility::HIDDEN:
      term_visibility = TerminationVisibility::kHidden;
      break;
    case content::Visibility::OCCLUDED:
      term_visibility = TerminationVisibility::kOccluded;
      break;
    case content::Visibility::VISIBLE:
      term_visibility = TerminationVisibility::kVisible;
      break;
  }
  base::UmaHistogramEnumeration("Stability.CommitLimitTerminatedTab.Visibility",
                                term_visibility);

  if (visibility == content::Visibility::HIDDEN) {
    // For background/hidden tabs, Chrome is expected to silently discard them.
    // We start tracking the reactivation/discard state.
    TransitionTo(TrackingState::kAwaitingReactivation);
  }
}

void CommitLimitOOMRecoveryTracker::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (state_ == TrackingState::kIdle) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!IsReloadNavigation(navigation_handle->GetURL())) {
    // User navigated away instead of reloading (censored path).
    TransitionTo(TrackingState::kIdle);
    return;
  }

  TransitionTo(TrackingState::kReloadInFlight);
}

void CommitLimitOOMRecoveryTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (state_ != TrackingState::kReloadInFlight) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // A successful commit that is not an error page indicates a recovery.
  if (navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage()) {
    RecordReloadResult(ReloadResult::kSuccess);
    TransitionTo(TrackingState::kIdle);
  }
  // If the navigation failed/cancelled but the renderer process is still
  // alive, we keep tracking and wait for the next attempt. We do not emit
  // and do not transition to kIdle.
}

void CommitLimitOOMRecoveryTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (state_ == TrackingState::kIdle) {
    return;
  }

  // Treat OCCLUDED same as VISIBLE.
  if (visibility == content::Visibility::VISIBLE ||
      visibility == content::Visibility::OCCLUDED) {
    if (state_ == TrackingState::kAwaitingReactivation) {
      if (web_contents()->WasDiscarded()) {
        TransitionTo(TrackingState::kReloadInFlight);
      } else {
        // Failed to discard background tab despite commit limit process
        // termination. This will result in a Sad Tab being shown when the tab
        // becomes visible.
        RecordReloadResult(ReloadResult::kFailedNoDiscard);
        TransitionTo(TrackingState::kIdle);
      }
    }
  } else if (visibility == content::Visibility::HIDDEN) {
    // Hide during tracking is treated as censored (emit nothing).
    TransitionTo(TrackingState::kIdle);
  }
}

bool CommitLimitOOMRecoveryTracker::IsTerminatedByCommitFailure() {
  // On Windows, check if the process termination was caused by a system commit
  // failure.
  int exit_code = web_contents()->GetCrashedErrorCode();
  return exit_code ==
         CHROME_RESULT_CODE_TERMINATED_BY_OTHER_PROCESS_ON_COMMIT_FAILURE;
}

// Check if the navigation is a reload. We match the URL against the last
// committed entry's redirect chain instead of checking GetReloadType()
// because discarded tab reactivations are initiated as history restores
// with ReloadType::NONE (to preserve page state and form data).
// We also avoid checking ExistingDocumentWasDiscarded() because it is transient
// and gets cleared on the first navigation request, which would miss retries
// if the initial automatic restore fails.
bool CommitLimitOOMRecoveryTracker::IsReloadNavigation(
    const GURL& navigation_url) {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }

  if (navigation_url.EqualsIgnoringRef(entry->GetURL())) {
    return true;
  }

  if (navigation_url.EqualsIgnoringRef(entry->GetOriginalRequestURL())) {
    return true;
  }

  for (const GURL& redirect_url : entry->GetRedirectChain()) {
    if (navigation_url.EqualsIgnoringRef(redirect_url)) {
      return true;
    }
  }

  return false;
}

void CommitLimitOOMRecoveryTracker::TransitionTo(TrackingState new_state) {
  state_ = new_state;
}
