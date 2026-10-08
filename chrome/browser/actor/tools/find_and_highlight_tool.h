// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"

class GURL;

namespace actor {

class ActorSurface;
class ActorTask;

// Highlights matching text in a tab and scrolls it into view.
class FindAndHighlightTool : public Tool {
 public:
  FindAndHighlightTool(TaskId task_id,
                       ToolDelegate& tool_delegate,
                       ActorSurface& actor_surface,
                       std::string query);
  ~FindAndHighlightTool() override;

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

  const std::string& query() const { return query_; }

 private:
  void OnHighlightFinished(ToolCallback callback, bool success);

  ActorSurfaceHandle actor_surface_handle_;
  std::string query_;

  base::WeakPtrFactory<FindAndHighlightTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_
