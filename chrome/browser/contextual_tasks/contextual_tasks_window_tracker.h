// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace content {
class WebContents;
struct OpenURLParams;
}

namespace contextual_tasks {

// Tracks the association between the mock <webview> window that was created in
// app.ts and the actual web contents that opened.
class ContextualTasksWindowTracker {
 public:
  ContextualTasksWindowTracker(
      const ContextualTaskId& task_id,
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

  // Called when the TabInterface becomes available for the tracked WebContents.
  void OnTabInterfaceAvailable(tabs::TabInterface* tab);

  // Called when the window is closed to notify the window tracker manager.
  void OnWindowClosed();

  // Accessors.
  const content::OpenURLParams* open_url_params() const;
  const ContextualTaskId& task_id() const { return task_id_; }
  const GURL& expected_url() const { return expected_url_; }
  content::WebContents* GetTabWebContents() const {
    if (web_contents_) {
      return web_contents_.get();
    }
    if (tab_) {
      return tab_->GetContents();
    }
    return nullptr;
  }
  const std::optional<ContextualWindowId>& window_id() const {
    return window_id_;
  }
  base::WeakPtr<content::WebContents> initiator_contents() const {
    return initiator_contents_;
  }
  base::WeakPtr<ContextualTasksWindowTracker> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetWindowId(ContextualWindowId window_id) { window_id_ = window_id; }
  void SetOpenURLParams(const content::OpenURLParams& params);

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // The ID of the task associated with this window tracking.
  ContextualTaskId task_id_;
  // The URL we expect the new window to load.
  GURL expected_url_;
  // The unique ID assigned to the tracked window.
  std::optional<ContextualWindowId> window_id_;
  // The WebContents that initiated the window opening.
  base::WeakPtr<content::WebContents> initiator_contents_;
  // The tab being tracked.
  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  // The WebContents being tracked (might not be in a tab yet).
  base::WeakPtr<content::WebContents> web_contents_;

  // The params stored from CanCreateWindow call.
  std::unique_ptr<content::OpenURLParams> open_url_params_;

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
