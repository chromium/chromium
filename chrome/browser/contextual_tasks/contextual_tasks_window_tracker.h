// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace content {
class WebContents;

}

namespace contextual_tasks {

// Tracks the association between the mock <webview> window that was created in
// app.ts and the actual web contents that opened.
class ContextualTasksWindowTracker {
 public:
  ContextualTasksWindowTracker(
      const base::Uuid& task_id,
      const GURL& expected_url,
      base::WeakPtr<content::WebContents> initiator_contents,
      base::OnceCallback<void(base::WeakPtr<ContextualTasksWindowTracker>)>
          on_closed_callback);
  ~ContextualTasksWindowTracker();
  ContextualTasksWindowTracker(const ContextualTasksWindowTracker&) = delete;
  ContextualTasksWindowTracker& operator=(const ContextualTasksWindowTracker&) =
      delete;

  // Assigns the tab's WebContents to be tracked.
  void SetTabWebContents(content::WebContents* web_contents);

  // Called when the window is closed to notify the window tracker manager.
  void OnWindowClosed();

  // Accessors.
  const base::Uuid& task_id() const { return task_id_; }
  const GURL& expected_url() const { return expected_url_; }
  content::WebContents* GetTabWebContents() const {
    return tab_ ? tab_->GetContents() : nullptr;
  }
  base::WeakPtr<content::WebContents> initiator_contents() const {
    return initiator_contents_;
  }
  base::WeakPtr<ContextualTasksWindowTracker> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // The ID of the task associated with this window tracking.
  base::Uuid task_id_;

  // The URL we expect the new window to load.
  GURL expected_url_;
  // The WebContents that initiated the window opening.
  base::WeakPtr<content::WebContents> initiator_contents_;
  // The tab being tracked.
  raw_ptr<tabs::TabInterface> tab_ = nullptr;

  // Subscriptions for tab events.
  base::CallbackListSubscription tab_subscription_;

  // Callback to run when the window is closed or timeout occurs.
  base::OnceCallback<void(base::WeakPtr<ContextualTasksWindowTracker>)>
      on_closed_callback_;
  // Timer to stop tracking after a timeout.
  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<ContextualTasksWindowTracker> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_
