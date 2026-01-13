// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_

#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  base::TimeTicks current_visit_start_time_;
  base::TimeDelta last_visit_duration_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_VISIT_TRACKER_H_
