// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/actor/core/shared_types.h"

namespace actor {

class ActorSurface;
class ActorTask;
class ToolDelegate;

// Executes an on-page search by locating a discoverable search input control in
// the page's cached AnnotatedPageContent and enqueuing a followup
// TypeToolRequest.
class SiteBasedSearchTool : public Tool {
 public:
  SiteBasedSearchTool(TaskId task_id,
                      ToolDelegate& tool_delegate,
                      ActorSurface& actor_surface,
                      std::string query,
                      std::string document_identifier);
  ~SiteBasedSearchTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  GURL JournalURL() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  ActorSurfaceHandle actor_surface_handle_;
  std::string query_;
  std::string document_identifier_;
  std::optional<DomNode> resolved_dom_node_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_H_
