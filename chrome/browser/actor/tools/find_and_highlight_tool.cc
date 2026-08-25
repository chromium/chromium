// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/find_and_highlight_tool.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

FindAndHighlightTool::FindAndHighlightTool(TaskId task_id,
                                           ToolDelegate& tool_delegate,
                                           tabs::TabHandle tab_handle,
                                           std::string query)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab_handle),
      query_(std::move(query)) {}

FindAndHighlightTool::~FindAndHighlightTool() = default;

void FindAndHighlightTool::Validate(ToolCallback callback) {
  CHECK(!query_.empty());

  if (!tab_handle_.Get()) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void FindAndHighlightTool::Invoke(ToolCallback callback) {
  // TODO(crbug.com/544815807): Implement.
  NOTIMPLEMENTED();
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string FindAndHighlightTool::DebugString() const {
  return base::StrCat({"FindAndHighlightTool[query=\"", query_, "\"]"});
}

std::string FindAndHighlightTool::JournalEvent() const {
  return "FindAndHighlight";
}

std::unique_ptr<ObservationDelayController>
FindAndHighlightTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // TODO(crbug.com/544815807): Implement.
  return nullptr;
}

tabs::TabHandle FindAndHighlightTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
