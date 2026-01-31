// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"

namespace contextual_tasks {

// Tracks the duration of the last foreground visit to a tab.
class ContextualTasksTabVisitTracker : public content::WebContentsObserver {
 public:
  explicit ContextualTasksTabVisitTracker(content::WebContents* contents);
  ~ContextualTasksTabVisitTracker() override;

  // Returns the duration of the last visit (the time the tab spent in the
  // foreground before it was last hidden). If the tab is currently in the
  // foreground, this returns the duration of the current visit.
  base::TimeDelta GetDurationOfCurrentOrLastVisit() const;

  // Returns the time elapsed since the tab was last active (in the foreground).
  // Returns std::nullopt if the tab has never been active. Returns 0 if the tab
  // is currently active.
  std::optional<base::TimeDelta> GetDurationSinceLastActive() const;

  // Injects a mock clock for testing.
  void SetClockForTesting(const base::TickClock* clock);

  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  // Check if a visit is currently in progress.
  bool IsCurrentlyVisiting() const {
    return !current_visit_start_time_.is_null();
  }

  // Check if there is a recorded previous visit.
  bool HasPreviousVisit() const {
    return !last_visit_end_time_.is_null();
  }

  base::TimeTicks current_visit_start_time_;
  base::TimeDelta last_visit_duration_;
  base::TimeTicks last_visit_end_time_;

  raw_ptr<const base::TickClock> clock_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_
