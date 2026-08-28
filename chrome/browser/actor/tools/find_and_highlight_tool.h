// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"

class GURL;

namespace actor {

class ActorTask;

// Highlights matching text in a tab and scrolls it into view.
class FindAndHighlightTool : public Tool {
 public:
  FindAndHighlightTool(TaskId task_id,
                       ToolDelegate& tool_delegate,
                       tabs::TabHandle tab_handle,
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
  tabs::TabHandle GetTargetTab() const override;

  const std::string& query() const { return query_; }
  tabs::TabHandle tab_handle() const { return tab_handle_; }

 private:
  void OnHighlightFinished(ToolCallback callback, bool success);

  tabs::TabHandle tab_handle_;
  std::string query_;

  base::WeakPtrFactory<FindAndHighlightTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_H_
