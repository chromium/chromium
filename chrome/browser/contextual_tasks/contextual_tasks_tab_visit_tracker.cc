// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"

#include "base/time/default_tick_clock.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace contextual_tasks {

DEFINE_USER_DATA(ContextualTasksTabVisitTracker);

ContextualTasksTabVisitTracker::ContextualTasksTabVisitTracker(
    tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this),
      clock_(base::DefaultTickClock::GetInstance()) {
  if (tab.GetContents()->GetVisibility() == content::Visibility::VISIBLE) {
    current_visit_start_time_ = clock_->NowTicks();
  }
}

ContextualTasksTabVisitTracker::~ContextualTasksTabVisitTracker() = default;

// static
ContextualTasksTabVisitTracker* ContextualTasksTabVisitTracker::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void ContextualTasksTabVisitTracker::SetClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
  if (!current_visit_start_time_.is_null()) {
    current_visit_start_time_ = clock_->NowTicks();
  }
}

void ContextualTasksTabVisitTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    current_visit_start_time_ = clock_->NowTicks();
  } else if (!current_visit_start_time_.is_null()) {
    base::TimeTicks now = clock_->NowTicks();
    last_visit_duration_ = now - current_visit_start_time_;
    last_visit_end_time_ = now;
    current_visit_start_time_ = base::TimeTicks();
  }
}

base::TimeDelta
ContextualTasksTabVisitTracker::GetDurationOfCurrentOrLastVisit() const {
  if (IsCurrentlyVisiting()) {
    return clock_->NowTicks() - current_visit_start_time_;
  }
  return last_visit_duration_;
}

std::optional<base::TimeDelta>
ContextualTasksTabVisitTracker::GetDurationSinceLastActive() const {
  if (IsCurrentlyVisiting()) {
    return base::TimeDelta();
  }
  if (HasPreviousVisit()) {
    return clock_->NowTicks() - last_visit_end_time_;
  }
  return std::nullopt;
}

}  // namespace contextual_tasks
