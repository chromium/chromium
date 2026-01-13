// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"

#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

ContextualTasksTabVisitTracker::ContextualTasksTabVisitTracker(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  if (contents->GetVisibility() == content::Visibility::VISIBLE) {
    current_visit_start_time_ = base::TimeTicks::Now();
  }
}

ContextualTasksTabVisitTracker::~ContextualTasksTabVisitTracker() = default;

void ContextualTasksTabVisitTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    current_visit_start_time_ = base::TimeTicks::Now();
  } else if (!current_visit_start_time_.is_null()) {
    last_visit_duration_ = base::TimeTicks::Now() - current_visit_start_time_;
    current_visit_start_time_ = base::TimeTicks();
  }
}

base::TimeDelta
ContextualTasksTabVisitTracker::GetDurationOfCurrentOrLastVisit() const {
  if (!current_visit_start_time_.is_null()) {
    return base::TimeTicks::Now() - current_visit_start_time_;
  }
  return last_visit_duration_;
}

}  // namespace contextual_tasks
