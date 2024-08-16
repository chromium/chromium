// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
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
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);
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
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);
  TransitionState(it, loading_state);
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
  tabs_.insert(std::make_pair(web_contents, data));
  ++state_counts_[static_cast<size_t>(data.loading_state)];

  for (Observer& observer : observers_)
    observer.OnStartTracking(web_contents, loading_state);
}

void TabLoadTracker::StopTracking(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = tabs_.find(web_contents);
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);

  auto loading_state = it->second.loading_state;
  DCHECK_NE(0u, state_counts_[static_cast<size_t>(it->second.loading_state)]);
  --state_counts_[static_cast<size_t>(it->second.loading_state)];
  tabs_.erase(it);

  for (Observer& observer : observers_)
    observer.OnStopTracking(web_contents, loading_state);
}

void TabLoadTracker::PrimaryPageChanged(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only observe top-level navigation that triggers navigation UI.
  if (!web_contents->ShouldShowLoadingUI())
    return;

  auto it = tabs_.find(web_contents);
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);
  TransitionState(it, LOADING);
}

void TabLoadTracker::DidStopLoading(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = tabs_.find(web_contents);
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);

  // Corner case: An unloaded tab that starts loading but never receives a
  // response transitions to the LOADED state when loading stops, without
  // traversing the LOADING state. This can happen when the server doesn't
  // respond or when there is no network connection.
  if (it->second.loading_state == LoadingState::UNLOADED)
    TransitionState(it, LOADED);
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
  CHECK(it != tabs_.end(), base::NotFatalUntil::M130);
  // The tab could already be UNLOADED if it hasn't yet started loading. This
  // can happen if the renderer crashes between the UNLOADED and LOADING states.
  if (it->second.loading_state == UNLOADED)
    return;
  TransitionState(it, UNLOADED);
}

void TabLoadTracker::OnPageStoppedLoading(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tabs_.find(web_contents);
  if (it == tabs_.end()) {
    // The graph tracks objects that are not tracked by this object.
    return;
  }

  TransitionState(it, LOADED);
}

TabLoadTracker::LoadingState TabLoadTracker::DetermineLoadingState(
    content::WebContents* web_contents) {
  // Determine if the WebContents is actively loading, using our definition of
  // loading. Start from the assumption that it is UNLOADED.
  LoadingState loading_state = UNLOADED;
  if (web_contents->ShouldShowLoadingUI() &&
      !web_contents->IsWaitingForResponse()) {
    loading_state = LOADING;
  } else {
    // Determine if the WebContents is already loaded. A loaded WebContents has
    // a committed navigation entry that is not the initial entry, is not in an
    // initial navigation, and doesn't require a reload. This can occur during
    // prerendering, when an already rendered WebContents is swapped in at the
    // moment of a navigation.
    content::NavigationController& controller = web_contents->GetController();
    if (!controller.GetLastCommittedEntry()->IsInitialEntry() &&
        !controller.IsInitialNavigation() && !controller.NeedsReload()) {
      loading_state = LOADED;
    }
  }

  return loading_state;
}

void TabLoadTracker::TransitionState(TabMap::iterator it,
                                     LoadingState loading_state) {
  LoadingState previous_state = it->second.loading_state;

  if (previous_state == loading_state) {
    return;
  }

  --state_counts_[static_cast<size_t>(previous_state)];
  it->second.loading_state = loading_state;
  ++state_counts_[static_cast<size_t>(loading_state)];

  // Store |it->first| instead of passing it directly in the loop below in case
  // an observer starts/stops tracking a WebContents and invalidates |it|.
  content::WebContents* web_contents = it->first;

  for (Observer& observer : observers_)
    observer.OnLoadingStateChange(web_contents, previous_state, loading_state);
}

TabLoadTracker::Observer::Observer() {}

TabLoadTracker::Observer::~Observer() {}

}  // namespace resource_coordinator
