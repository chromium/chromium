// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace actor {

class ActorSurface;
class TabManagementTool;

// Navigates the primary main frame in a WebContents to the given URL, or opens
// a new foreground tab in a window, adds it to the task, and navigates it to
// the given URL. In the latter case the navigation itself runs as a follow-up
// `NavigateToolRequest` (see `ToolDelegate::EnqueueFollowupAction()`) on the
// new tab, whose result becomes the result of this action.
class NavigateTool : public Tool, content::WebContentsObserver {
 public:
  // Navigates `actor_surface` to `url`.
  NavigateTool(TaskId task_id,
               ToolDelegate& tool_delegate,
               ActorSurface& actor_surface,
               const GURL& url);
  // Opens a new foreground tab in `window_id` and navigates it to `url`.
  NavigateTool(TaskId task_id,
               ToolDelegate& tool_delegate,
               SessionID window_id,
               const GURL& url);
  ~NavigateTool() override;

  // actor::Tool
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  void UpdateTaskAfterInvoke(ActorTask& task,
                             mojom::ActionResultPtr result,
                             ToolCallback callback) const override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Called when `create_tab_tool_` finishes. On success, enqueues the new
  // tab's navigation to `url_`. The tab is added to the task in
  // `UpdateTaskAfterInvoke()`, before the navigation starts.
  void OnTabCreated(ToolCallback callback, mojom::ActionResultPtr result);
  void NavigationHandleCallback(content::NavigationHandle& handle);

  GURL url_;

  // The window to open a new foreground tab in. Only set when opening a new
  // tab, in which case `create_tab_tool_` creates it.
  // TODO(b/555808355): Replace this delegation once composite tools are
  // supported.
  std::optional<SessionID> window_id_;
  std::unique_ptr<TabManagementTool> create_tab_tool_;

  // Holds the callback to the Invoke method. Null before invoke is called.
  ToolCallback invoke_callback_;

  // The ID of the navigation to `url_`, unset until the navigation is started,
  // after which this is set (asynchronously). Once set, this class observes the
  // WebContents until this handle completes and the above callback is invoked.
  std::optional<int64_t> pending_navigation_handle_id_;

  // The navigated surface. When opening a new tab, this is null until the tab
  // has been created.
  ActorSurfaceHandle actor_surface_handle_;

  base::WeakPtrFactory<NavigateTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
