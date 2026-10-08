// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace actor {

class ActorSurface;

// Navigates a the primary main frame in a WebContents to the given URL.
class NavigateTool : public Tool, content::WebContentsObserver {
 public:
  NavigateTool(TaskId task_id,
               ToolDelegate& tool_delegate,
               ActorSurface& actor_surface,
               const GURL& url);
  ~NavigateTool() override;

  // actor::Tool
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void NavigationHandleCallback(content::NavigationHandle& handle);

  GURL url_;

  // Holds the callback to the Invoke method. Null before invoke is called.
  ToolCallback invoke_callback_;

  // The ID of the navigation to `url_`, unset until the navigation is started,
  // after which this is set (asynchronously). Once set, this class observes the
  // WebContents until this handle completes and the above callback is invoked.
  std::optional<int64_t> pending_navigation_handle_id_;

  ActorSurfaceHandle actor_surface_handle_;

  base::WeakPtrFactory<NavigateTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
