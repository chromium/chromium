// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/actor/core/task_id.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

class ActorSurface;
class ActorTask;
class TabManagementTool;
class ToolDelegate;

// Opens `url_` relative to a given actor surface. The navigation itself is
// performed by a follow-up `NavigateToolRequest` (see
// `ToolDelegate::EnqueueFollowupAction()`), whose result becomes the result of
// this action:
//   - If `open_in_new_tab_` is false, the surface is navigated to `url_`.
//   - If `open_in_new_tab_` is true, a new foreground tab is first opened in
//     the surface's window via an owned `TabManagementTool` and added to the
//     task, then navigated to `url_`. The new tab stays in the task (e.g.
//     showing the error page) even if the navigation fails.
class OpenUrlTool : public Tool {
 public:
  OpenUrlTool(TaskId task_id,
              ToolDelegate& tool_delegate,
              const ActorSurface& actor_surface,
              const GURL& url,
              bool open_in_new_tab);
  OpenUrlTool(const OpenUrlTool&) = delete;
  OpenUrlTool& operator=(const OpenUrlTool&) = delete;
  ~OpenUrlTool() override;

  // actor::Tool:
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

 private:
  // Returns the window containing the surface's tab, or nullptr.
  BrowserWindowInterface* GetSurfaceBrowserWindow();

  // Called when `create_tab_tool_` finishes. On success, enqueues the new
  // tab's navigation to `url_`; the tab is added to the task in
  // `UpdateTaskAfterInvoke()`, before the navigation starts.
  void OnTabCreated(ToolCallback callback, mojom::ActionResultPtr result);

  // Enqueues a follow-up action that navigates `target` to `url_`.
  void EnqueueNavigation(ActorSurfaceHandle target);

  const GURL url_;
  const bool open_in_new_tab_;

  // The surface this tool was created for.
  const ActorSurfaceHandle actor_surface_handle_;

  // The new tab, set once it has been created (only if `open_in_new_tab_`).
  ActorSurfaceHandle new_tab_surface_;

  // Creates the new tab (only if `open_in_new_tab_`).
  // TODO(b/555808355): Replace this delegation once composite tools are
  // supported.
  std::unique_ptr<TabManagementTool> create_tab_tool_;

  base::WeakPtrFactory<OpenUrlTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_H_
