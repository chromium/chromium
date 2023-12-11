// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"

#include "base/containers/contains.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"

namespace performance_manager::policies {

namespace {
HighEfficiencyModePolicy* g_high_efficiency_mode_policy = nullptr;

HighEfficiencyModePolicy::MemorySaverMode GetCurrentMode() {
  int mode_value = performance_manager::features::kModalMemorySaverMode.Get();
  CHECK_GE(mode_value, 0);
  CHECK_LE(
      mode_value,
      static_cast<int>(HighEfficiencyModePolicy::MemorySaverMode::kMaxValue));
  return static_cast<HighEfficiencyModePolicy::MemorySaverMode>(mode_value);
}
}  // namespace

HighEfficiencyModePolicy::HighEfficiencyModePolicy()
    : time_before_discard_(base::TimeDelta::Max()) {
  DCHECK(!g_high_efficiency_mode_policy);
  g_high_efficiency_mode_policy = this;
}

HighEfficiencyModePolicy::~HighEfficiencyModePolicy() {
  DCHECK_EQ(this, g_high_efficiency_mode_policy);
  g_high_efficiency_mode_policy = nullptr;
}

// static
HighEfficiencyModePolicy* HighEfficiencyModePolicy::GetInstance() {
  return g_high_efficiency_mode_policy;
}

void HighEfficiencyModePolicy::OnIsVisibleChanged(const PageNode* page_node) {
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

void HighEfficiencyModePolicy::OnTabAdded(
    TabPageDecorator::TabHandle* tab_handle) {
  if (!tab_handle->page_node()->IsVisible()) {
    // Some mechanisms (like "session restore" and "open all bookmarks") can
    // create pages that are non-visible. If that happens, start a discard timer
    // so that the pages are discarded if they don't ever become visible.
    // TODO(crbug.com/1510539): High Efficiency Mode should make it so
    // non-visible pages are simply not loaded until they become visible.
    StartDiscardTimerIfEnabled(tab_handle,
                               GetTimeBeforeDiscardForCurrentMode());
  }
}

void HighEfficiencyModePolicy::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  RemoveActiveTimer(tab_handle);
}

void HighEfficiencyModePolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  graph->AddPageNodeObserver(this);
  graph->GetRegisteredObjectAs<TabPageDecorator>()->AddObserver(this);
}

void HighEfficiencyModePolicy::OnTakenFromGraph(Graph* graph) {
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
  graph_ = nullptr;
}

void HighEfficiencyModePolicy::OnHighEfficiencyModeChanged(bool enabled) {
  high_efficiency_mode_enabled_ = enabled;

  if (high_efficiency_mode_enabled_) {
    DCHECK(active_discard_timers_.empty());
    StartAllDiscardTimers();
  } else {
    active_discard_timers_.clear();
  }
}

base::TimeDelta HighEfficiencyModePolicy::GetTimeBeforeDiscardForTesting()
    const {
  return GetTimeBeforeDiscardForCurrentMode();
}

void HighEfficiencyModePolicy::SetTimeBeforeDiscard(
    base::TimeDelta time_before_discard) {
  time_before_discard_ = time_before_discard;
  if (high_efficiency_mode_enabled_) {
    active_discard_timers_.clear();
    StartAllDiscardTimers();
  }
}

bool HighEfficiencyModePolicy::IsHighEfficiencyDiscardingEnabled() const {
  return high_efficiency_mode_enabled_;
}

void HighEfficiencyModePolicy::StartAllDiscardTimers() {
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    TabPageDecorator::TabHandle* tab_handle =
        TabPageDecorator::FromPageNode(page_node);
    if (tab_handle && !page_node->IsVisible()) {
      StartDiscardTimerIfEnabled(tab_handle,
                                 GetTimeBeforeDiscardForCurrentMode());
    }
  }
}

void HighEfficiencyModePolicy::StartDiscardTimerIfEnabled(
    const TabPageDecorator::TabHandle* tab_handle,
    base::TimeDelta time_before_discard) {
  if (IsHighEfficiencyDiscardingEnabled()) {
    TabRevisitTracker* revisit_tracker =
        graph_->GetRegisteredObjectAs<TabRevisitTracker>();
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

    // High Efficiency mode is enabled, so the tab should be discarded after the
    // amount of time specified by finch is elapsed.
    CHECK_NE(time_before_discard, base::TimeDelta::Max());
    active_discard_timers_[tab_handle].Start(
        FROM_HERE, time_before_discard,
        base::BindOnce(&HighEfficiencyModePolicy::DiscardPageTimerCallback,
                       base::Unretained(this), tab_handle,
                       base::LiveTicks::Now(), time_before_discard));
  }
}

void HighEfficiencyModePolicy::RemoveActiveTimer(
    const TabPageDecorator::TabHandle* tab_handle) {
  // If there's a discard timer already running for this page, erase it from the
  // map which will stop the timer when it is destroyed.
  active_discard_timers_.erase(tab_handle);
}

void HighEfficiencyModePolicy::DiscardPageTimerCallback(
    const TabPageDecorator::TabHandle* tab_handle,
    base::LiveTicks posted_at,
    base::TimeDelta requested_time_before_discard) {
  // When this callback is invoked, the `tab_handle` is guaranteed to still be
  // valid otherwise `OnBeforeTabRemoved` would've been called and the
  // timer destroyed.
  RemoveActiveTimer(tab_handle);

  // Turning off High Efficiency Mode would delete the timer, so it's not
  // possible to get here and for High Efficiency Mode to be off.
  DCHECK(IsHighEfficiencyDiscardingEnabled());

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
    PageDiscardingHelper::GetFromGraph(graph_)->ImmediatelyDiscardSpecificPage(
        tab_handle->page_node(),
        PageDiscardingHelper::DiscardReason::PROACTIVE);
  }
}

base::TimeDelta HighEfficiencyModePolicy::GetTimeBeforeDiscardForCurrentMode()
    const {
  if (base::FeatureList::IsEnabled(features::kModalMemorySaver)) {
    MemorySaverMode mode = GetCurrentMode();

    if (mode == MemorySaverMode::kConservative) {
      return base::Hours(6);
    } else if (mode == MemorySaverMode::kMedium) {
      return base::Hours(4);
    } else if (mode == MemorySaverMode::kAggressive) {
      return base::Hours(2);
    }
  }

  return time_before_discard_;
}

int HighEfficiencyModePolicy::GetMaxNumRevisitsForCurrentMode() const {
  MemorySaverMode mode = GetCurrentMode();

  if (mode == MemorySaverMode::kConservative) {
    return 15;
  } else if (mode == MemorySaverMode::kMedium) {
    return 15;
  } else if (mode == MemorySaverMode::kAggressive) {
    return 5;
  }

  return std::numeric_limits<int>::max();
}

}  // namespace performance_manager::policies
