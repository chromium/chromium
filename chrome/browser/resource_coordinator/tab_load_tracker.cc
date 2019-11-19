// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace resource_coordinator {

namespace {

static constexpr TabLoadTracker::LoadingState UNLOADED =
    TabLoadTracker::LoadingState::UNLOADED;
static constexpr TabLoadTracker::LoadingState LOADING =
    TabLoadTracker::LoadingState::LOADING;
static constexpr TabLoadTracker::LoadingState LOADED =
    TabLoadTracker::LoadingState::LOADED;

}  // namespace

TabLoadTracker::~TabLoadTracker() = default;

// static
TabLoadTracker* TabLoadTracker::Get() {
  DCHECK(g_browser_process);
  return g_browser_process->resource_coordinator_parts()->tab_load_tracker();
}

TabLoadTracker::LoadingState TabLoadTracker::GetLoadingState(
    content::WebContents* web_contents) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  return it->second.loading_state;
}

size_t TabLoadTracker::GetTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tabs_.size();
}

size_t TabLoadTracker::GetTabCount(LoadingState loading_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_counts_[static_cast<size_t>(loading_state)];
}

size_t TabLoadTracker::GetUnloadedTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_counts_[static_cast<size_t>(UNLOADED)];
}

size_t TabLoadTracker::GetLoadingTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_counts_[static_cast<size_t>(LOADING)];
}

size_t TabLoadTracker::GetLoadedTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_counts_[static_cast<size_t>(LOADED)];
}

size_t TabLoadTracker::GetUiTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_tab_state_counts_[static_cast<size_t>(UNLOADED)] +
         ui_tab_state_counts_[static_cast<size_t>(LOADING)] +
         ui_tab_state_counts_[static_cast<size_t>(LOADED)];
}

size_t TabLoadTracker::GetUiTabCount(LoadingState loading_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_tab_state_counts_[static_cast<size_t>(loading_state)];
}

size_t TabLoadTracker::GetUnloadedUiTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_tab_state_counts_[static_cast<size_t>(UNLOADED)];
}

size_t TabLoadTracker::GetLoadingUiTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_tab_state_counts_[static_cast<size_t>(LOADING)];
}

size_t TabLoadTracker::GetLoadedUiTabCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_tab_state_counts_[static_cast<size_t>(LOADED)];
}

void TabLoadTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void TabLoadTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void TabLoadTracker::TransitionStateForTesting(
    content::WebContents* web_contents,
    LoadingState loading_state) {
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  TransitionState(it, loading_state, false);
}

TabLoadTracker::TabLoadTracker() = default;

void TabLoadTracker::StartTracking(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(tabs_, web_contents));

  LoadingState loading_state = DetermineLoadingState(web_contents);

  // Insert the tab, making sure it's state is consistent with the valid states
  // documented in TransitionState.
  WebContentsData data;
  data.loading_state = loading_state;
  if (data.loading_state == LOADING)
    data.did_start_loading_seen = true;
  data.is_ui_tab = IsUiTab(web_contents);
  tabs_.insert(std::make_pair(web_contents, data));
  ++state_counts_[static_cast<size_t>(data.loading_state)];
  if (data.is_ui_tab)
    ++ui_tab_state_counts_[static_cast<size_t>(data.loading_state)];

  for (Observer& observer : observers_)
    observer.OnStartTracking(web_contents, loading_state);
}

void TabLoadTracker::StopTracking(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());

  auto loading_state = it->second.loading_state;
  DCHECK_NE(0u, state_counts_[static_cast<size_t>(it->second.loading_state)]);
  --state_counts_[static_cast<size_t>(it->second.loading_state)];
  if (it->second.is_ui_tab) {
    DCHECK_NE(
        0u,
        ui_tab_state_counts_[static_cast<size_t>(it->second.loading_state)]);
    --ui_tab_state_counts_[static_cast<size_t>(it->second.loading_state)];
  }
  tabs_.erase(it);

  for (Observer& observer : observers_)
    observer.OnStopTracking(web_contents, loading_state);
}

void TabLoadTracker::DidStartLoading(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents->IsLoadingToDifferentDocument())
    return;
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  if (it->second.loading_state == LOADING) {
    DCHECK(it->second.did_start_loading_seen);
    return;
  }
  it->second.did_start_loading_seen = true;
}

void TabLoadTracker::DidReceiveResponse(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  if (it->second.loading_state == LOADING) {
    DCHECK(it->second.did_start_loading_seen);
    return;
  }
  // A transition to loading requires both DidStartLoading (navigation
  // committed) and DidReceiveResponse (data has been trasmitted over the
  // network) events to occur. This is because NavigationThrottles can block
  // actual network requests, but not the rest of the state machinery.
  if (!it->second.did_start_loading_seen)
    return;
  TransitionState(it, LOADING, true);
}

void TabLoadTracker::DidFailLoad(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeTransitionToLoaded(web_contents);
}

void TabLoadTracker::RenderProcessGone(content::WebContents* web_contents,
                                       base::TerminationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't bother tracking the UNLOADED state change for normal renderer
  // shutdown, the |web_contents| will be untracked shortly.
  if (status ==
          base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }
  // We reach here when a tab crashes, i.e. it's main frame renderer dies
  // unexpectedly (sad tab). In this case there is still an associated
  // WebContents, but it is not backed by a renderer. The renderer could have
  // died because of a crash (e.g. bugs, compromised renderer) or been killed by
  // the OS (e.g. OOM on Android). Note: discarded tabs may reach this method,
  // but exit early because of |status|.
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  // The tab could already be UNLOADED if it hasn't yet started loading. This
  // can happen if the renderer crashes between the UNLOADED and LOADING states.
  if (it->second.loading_state == UNLOADED)
    return;
  TransitionState(it, UNLOADED, true);
}

void TabLoadTracker::OnPageAlmostIdle(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TabManager::ResourceCoordinatorSignalObserver filters late notifications
  // so here we can assume the event pertains to a live web_contents and
  // its most recent navigation. However, the graph tracks contents that aren't
  // tracked by this object.
  if (!base::Contains(tabs_, web_contents))
    return;

  MaybeTransitionToLoaded(web_contents);
}

TabLoadTracker::LoadingState TabLoadTracker::DetermineLoadingState(
    content::WebContents* web_contents) {
  // Determine if the WebContents is actively loading, using our definition of
  // loading. Start from the assumption that it is UNLOADED.
  LoadingState loading_state = UNLOADED;
  if (web_contents->IsLoadingToDifferentDocument() &&
      !web_contents->IsWaitingForResponse()) {
    loading_state = LOADING;
  } else {
    // Determine if the WebContents is already loaded. A loaded WebContents has
    // a committed navigation entry, is not in an initial navigation, and
    // doesn't require a reload. This can occur during prerendering, when an
    // already rendered WebContents is swapped in at the moment of a navigation.
    content::NavigationController& controller = web_contents->GetController();
    if (controller.GetLastCommittedEntry() != nullptr &&
        !controller.IsInitialNavigation() && !controller.NeedsReload()) {
      loading_state = LOADED;
    }
  }

  return loading_state;
}

void TabLoadTracker::MaybeTransitionToLoaded(
    content::WebContents* web_contents) {
  auto it = tabs_.find(web_contents);
  DCHECK(it != tabs_.end());
  if (it->second.loading_state != LOADING)
    return;
  TransitionState(it, LOADED, true);
}

void TabLoadTracker::TransitionState(TabMap::iterator it,
                                     LoadingState loading_state,
                                     bool validate_transition) {
#if DCHECK_IS_ON()
  if (validate_transition) {
    // Validate the transition.
    switch (loading_state) {
      case LOADING: {
        DCHECK_NE(LOADING, it->second.loading_state);
        DCHECK(it->second.did_start_loading_seen);
        break;
      }

      case LOADED: {
        DCHECK_EQ(LOADING, it->second.loading_state);
        DCHECK(it->second.did_start_loading_seen);
        break;
      }

      case UNLOADED: {
        DCHECK_NE(UNLOADED, it->second.loading_state);
        break;
      }
    }
  }
#endif

  LoadingState previous_state = it->second.loading_state;
  --state_counts_[static_cast<size_t>(previous_state)];
  it->second.loading_state = loading_state;
  ++state_counts_[static_cast<size_t>(loading_state)];
  if (it->second.is_ui_tab) {
    ++ui_tab_state_counts_[static_cast<size_t>(loading_state)];
    DCHECK_NE(0u, ui_tab_state_counts_[static_cast<size_t>(previous_state)]);
    --ui_tab_state_counts_[static_cast<size_t>(previous_state)];
  }

  // If the destination state is LOADED, then also clear the
  // |did_start_loading_seen| state.
  if (loading_state == LOADED)
    it->second.did_start_loading_seen = false;

  // Store |it->first| instead of passing it directly in the loop below in case
  // an observer starts/stops tracking a WebContents and invalidates |it|.
  content::WebContents* web_contents = it->first;

  for (Observer& observer : observers_)
    observer.OnLoadingStateChange(web_contents, previous_state, loading_state);
}

bool TabLoadTracker::IsUiTab(content::WebContents* web_contents) {
  // TODO(crbug.com/836409): This should be able to check directly with the
  // tabstrip UI or use a platform-independent tabstrip observer interface to
  // learn about |web_contents| associated with the tabstrip, rather than
  // checking for specific cases where |web_contents| is not a ui tab.
  if (prerender::PrerenderContents::FromWebContents(web_contents) != nullptr)
    return false;
  if (web_contents->GetOuterWebContents())
    return false;
  return true;
}

void TabLoadTracker::SwapTabContents(content::WebContents* old_contents,
                                     content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/836409): This should work by directly tracking tabs that are
  // attached to UI surfaces instead of relying on being notified directly about
  // tab contents swaps.

  // Transition |old_contents| to a non-UI tab. If a tab is being swapped out,
  // then it should exist, we should be tracking it, and it should be a UI tab.
  DCHECK(old_contents);
  auto it = tabs_.find(old_contents);
  DCHECK(it != tabs_.end());
  DCHECK(it->second.is_ui_tab);
  it->second.is_ui_tab = false;
  DCHECK_NE(
      0u, ui_tab_state_counts_[static_cast<size_t>(it->second.loading_state)]);
  --ui_tab_state_counts_[static_cast<size_t>(it->second.loading_state)];

  // Transition |new_contents| to a UI tab.
  DCHECK(IsUiTab(new_contents));
  it = tabs_.find(new_contents);
  // |new_contents| will not be tracked if a tab helper wasn't attached yet,
  // which currently happens for dom distiller. In this case, the tab helper
  // will be attached and we will start tracking it when it's swapped into the
  // tab UI, which will happen later in this code path.
  if (it == tabs_.end())
    return;

  // |new_contents| shouldn't be considered a UI tab yet. This should catch any
  // new cases of non-tab web contents that attach tab helpers that we aren't
  // handling.
  DCHECK(!it->second.is_ui_tab);
  // Promote |new_contents| to a UI tab.
  it->second.is_ui_tab = true;
  ++ui_tab_state_counts_[static_cast<size_t>(it->second.loading_state)];
}

TabLoadTracker::Observer::Observer() {}

TabLoadTracker::Observer::~Observer() {}

}  // namespace resource_coordinator
