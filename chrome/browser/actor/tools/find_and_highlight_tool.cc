// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/find_and_highlight_tool.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tab_annotation_manager.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

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
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab || !tab->GetContents()) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  content::WebContents* web_contents = tab->GetContents();
  auto* annotation_manager =
      TabAnnotationManager::GetOrCreateForWebContents(web_contents);
  CHECK(annotation_manager);

  annotation_manager->HighlightText(
      query_,
      base::BindOnce(&FindAndHighlightTool::OnHighlightFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FindAndHighlightTool::OnHighlightFinished(ToolCallback callback,
                                               bool success) {
  if (success) {
    PostResponseTask(std::move(callback), MakeOkResult());
    return;
  }

  PostResponseTask(
      std::move(callback),
      MakeResult(mojom::ActionResultCode::kFindAndHighlightTextNotFound,
                 /*requires_page_stabilization=*/false,
                 "Failed to find or highlight matching text."));
}

std::string FindAndHighlightTool::DebugString() const {
  return base::StrCat({"FindAndHighlightTool[query=\"", query_, "\"]"});
}

std::string FindAndHighlightTool::JournalEvent() const {
  return "FindAndHighlight";
}

GURL FindAndHighlightTool::JournalURL() const {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (tab && tab->GetContents()) {
    return tab->GetContents()->GetLastCommittedURL();
  }
  return GURL();
}

std::unique_ptr<ObservationDelayController>
FindAndHighlightTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

void FindAndHighlightTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                                  ToolCallback callback) const {
  task.AddTab(tab_handle_, /*stop_task_on_detach=*/true, std::move(callback));
}

tabs::TabHandle FindAndHighlightTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
