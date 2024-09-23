// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/memory_saver_mode_policy.h"

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"

namespace performance_manager::policies {

namespace {
MemorySaverModePolicy* g_memory_saver_mode_policy = nullptr;
using user_tuning::prefs::MemorySaverModeAggressiveness;
}  // namespace

MemorySaverModePolicy::MemorySaverModePolicy() {
  DCHECK(!g_memory_saver_mode_policy);
  g_memory_saver_mode_policy = this;
}

MemorySaverModePolicy::~MemorySaverModePolicy() {
  DCHECK_EQ(this, g_memory_saver_mode_policy);
  g_memory_saver_mode_policy = nullptr;
}

// static
MemorySaverModePolicy* MemorySaverModePolicy::GetInstance() {
  return g_memory_saver_mode_policy;
}

void MemorySaverModePolicy::OnIsVisibleChanged(const PageNode* page_node) {
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  if (!tab_handle) {
    return;
  }

  // If the page is made visible, any existing timers that refer to it should be
  // cancelled. `RemoveActiveTimer` handles the case where no timer exists
  // gracefully.
  if (page_node->IsVisible()) {
    RemoveActiveTimer(tab_handle);
  } else {
    StartDiscardTimerIfEnabled(tab_handle,
                               GetTimeBeforeDiscardForCurrentMode());
  }
}

void MemorySaverModePolicy::OnTabAdded(
    TabPageDecorator::TabHandle* tab_handle) {
  if (!tab_handle->page_node()->IsVisible()) {
    // Some mechanisms (like "session restore" and "open all bookmarks") can
    // create pages that are non-visible. If that happens, start a discard timer
    // so that the pages are discarded if they don't ever become visible.
    // TODO(crbug.com/41483135): Memory Saver Mode should make it so
    // non-visible pages are simply not loaded until they become visible.
    StartDiscardTimerIfEnabled(tab_handle,
                               GetTimeBeforeDiscardForCurrentMode());
  }
}

void MemorySaverModePolicy::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  RemoveActiveTimer(tab_handle);
}

void MemorySaverModePolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->GetRegisteredObjectAs<TabPageDecorator>()->AddObserver(this);
}

void MemorySaverModePolicy::OnTakenFromGraph(Graph* graph) {
  // The logic in this class depends on being notified of pages being removed,
  // otherwise there's no guarantee PageNode pointers are still valid when
  // timers fire. To avoid possibly having callbacks manipulate invalid PageNode
  // pointers, clear all the existing timers before unregistering the observer.
  active_discard_timers_.clear();

  // Decorator destruction ordered is not defined, so only unregister from
  // `TabPageDecorator` if it's still present.
  auto* tab_page_decorator = graph->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }
  graph->RemovePageNodeObserver(this);
}

void MemorySaverModePolicy::OnMemorySaverModeChanged(bool enabled) {
  high_efficiency_mode_enabled_ = enabled;

  if (high_efficiency_mode_enabled_) {
    DCHECK(active_discard_timers_.empty());
    StartAllDiscardTimers();
  } else {
    active_discard_timers_.clear();
  }
}

base::TimeDelta MemorySaverModePolicy::GetTimeBeforeDiscardForTesting() const {
  return GetTimeBeforeDiscardForCurrentMode();
}

void MemorySaverModePolicy::SetMode(MemorySaverModeAggressiveness mode) {
  mode_ = mode;
  if (high_efficiency_mode_enabled_) {
    active_discard_timers_.clear();
    StartAllDiscardTimers();
  }
}

bool MemorySaverModePolicy::IsMemorySaverDiscardingEnabled() const {
  return high_efficiency_mode_enabled_;
}

void MemorySaverModePolicy::StartAllDiscardTimers() {
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    TabPageDecorator::TabHandle* tab_handle =
        TabPageDecorator::FromPageNode(page_node);
    if (tab_handle && !page_node->IsVisible()) {
      StartDiscardTimerIfEnabled(tab_handle,
                                 GetTimeBeforeDiscardForCurrentMode());
    }
  }
}

void MemorySaverModePolicy::StartDiscardTimerIfEnabled(
    const TabPageDecorator::TabHandle* tab_handle,
    base::TimeDelta time_before_discard) {
  if (IsMemorySaverDiscardingEnabled()) {
    TabRevisitTracker* revisit_tracker =
        GetOwningGraph()->GetRegisteredObjectAs<TabRevisitTracker>();
    CHECK(revisit_tracker);

    TabRevisitTracker::StateBundle state =
        revisit_tracker->GetStateForTabHandle(tab_handle);

    if (state.num_revisits > GetMaxNumRevisitsForCurrentMode()) {
      // Don't start the discard timer if the tab has been revisited more times
      // than the max allowable for the current mode. It's impossible for
      // `num_revisits` to decrease. This condition is only checked here (not in
      // the timer callback) because `num_revisits` can only increase by
      // revisiting the tab, which would delete the timer.
      return;
    }

    // Memory Saver mode is enabled, so the tab should be discarded after the
    // amount of time specified by finch is elapsed.
    CHECK_NE(time_before_discard, base::TimeDelta::Max());
    active_discard_timers_[tab_handle].Start(
        FROM_HERE, time_before_discard,
        base::BindOnce(&MemorySaverModePolicy::DiscardPageTimerCallback,
                       base::Unretained(this), tab_handle,
                       base::LiveTicks::Now(), time_before_discard));
  }
}

void MemorySaverModePolicy::RemoveActiveTimer(
    const TabPageDecorator::TabHandle* tab_handle) {
  // If there's a discard timer already running for this page, erase it from the
  // map which will stop the timer when it is destroyed.
  active_discard_timers_.erase(tab_handle);
}

void MemorySaverModePolicy::DiscardPageTimerCallback(
    const TabPageDecorator::TabHandle* tab_handle,
    base::LiveTicks posted_at,
    base::TimeDelta requested_time_before_discard) {
  // When this callback is invoked, the `tab_handle` is guaranteed to still be
  // valid otherwise `OnBeforeTabRemoved` would've been called and the
  // timer destroyed.
  RemoveActiveTimer(tab_handle);

  // Turning off Memory Saver Mode would delete the timer, so it's not
  // possible to get here and for Memory Saver Mode to be off.
  DCHECK(IsMemorySaverDiscardingEnabled());

  // If the time elapsed according to `LiveTicks` is shorter than
  // `requested_time_before_discard`, it means that the device was in a
  // suspended state at some point between when the timer was started and now.
  // In this case, start a new timer for the difference, which is the remaining
  // time the tab should stay backgrounded to total
  // `requested_time_before_discard` in background.
  base::TimeDelta elapsed_not_suspended = base::LiveTicks::Now() - posted_at;
  if (elapsed_not_suspended < requested_time_before_discard) {
    StartDiscardTimerIfEnabled(
        tab_handle, requested_time_before_discard - elapsed_not_suspended);
  } else {
    GetOwningGraph()
        ->GetRegisteredObjectAs<PageDiscardingHelper>()
        ->ImmediatelyDiscardMultiplePages(
            {tab_handle->page_node()},
            PageDiscardingHelper::DiscardReason::PROACTIVE);
  }
}

base::TimeDelta MemorySaverModePolicy::GetTimeBeforeDiscardForCurrentMode()
    const {
  switch (mode_) {
    case MemorySaverModeAggressiveness::kConservative:
      return base::Hours(6);
    case MemorySaverModeAggressiveness::kMedium:
      return base::Hours(4);
    case MemorySaverModeAggressiveness::kAggressive:
      return base::Hours(2);
  }
  NOTREACHED();
}

int MemorySaverModePolicy::GetMaxNumRevisitsForCurrentMode() const {
  switch (mode_) {
    case MemorySaverModeAggressiveness::kConservative:
      return 15;
    case MemorySaverModeAggressiveness::kMedium:
      return 15;
    case MemorySaverModeAggressiveness::kAggressive:
      return 5;
  }
  NOTREACHED();
}

}  // namespace performance_manager::policies
