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
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
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
      content::GlobalRenderFrameHostToken initiator_rfh_token,
      base::WeakPtr<content::WebContents> webui_contents,
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
  content::WebContents* message_proxy_web_contents() const {
    return message_proxy_web_contents_.get();
  }
  const std::optional<ContextualWindowId>& window_id() const {
    return window_id_;
  }

  base::WeakPtr<content::WebContents> webui_contents() const {
    return webui_contents_;
  }
  base::WeakPtr<content::WebContents> initiator_contents() const;
  content::RenderFrameHost* GetInitiatorFrame() const;
  base::WeakPtr<ContextualTasksWindowTracker> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetWindowId(ContextualWindowId window_id) { window_id_ = window_id; }
  void SetOpenURLParams(const content::OpenURLParams& params);
  void SetWindowFeatures(const blink::mojom::WindowFeatures& features) {
    window_features_ = features;
  }
  const blink::mojom::WindowFeatures& window_features() const {
    return window_features_;
  }
  void SetMessageProxyWebContents(
      std::unique_ptr<content::WebContents> contents);

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // The ID of the task associated with this window tracking.
  ContextualTaskId task_id_;
  // The URL we expect the new window to load.
  GURL expected_url_;
  // The unique ID assigned to the tracked window.
  std::optional<ContextualWindowId> window_id_;
  // The token of the RenderFrameHost that initiated the window opening.
  content::GlobalRenderFrameHostToken initiator_rfh_token_;
  // The tab being tracked.
  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  // The WebContents being tracked (might not be in a tab yet).
  base::WeakPtr<content::WebContents> web_contents_;
  // The WebUI contents that is tracking this window.
  base::WeakPtr<content::WebContents> webui_contents_;
  // The params stored from CanCreateWindow call.
  std::unique_ptr<content::OpenURLParams> open_url_params_;
  // The window features stored from CanCreateWindow call.
  blink::mojom::WindowFeatures window_features_;
  // The dummy WebContents used as opener for message routing. For more
  // context in how WebContents routing works, see http://shortn/_eyIJvb0jIx.
  std::unique_ptr<content::WebContents> message_proxy_web_contents_;
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
