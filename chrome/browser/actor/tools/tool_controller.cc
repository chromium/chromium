// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/page_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::proto::Action;
using tabs::TabInterface;

namespace actor {

ToolController::ActiveState::ActiveState(
    std::unique_ptr<Tool> tool,
    ResultCallback completion_callback,
    content::WeakDocumentPtr weak_document_ptr,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)),
      weak_document_ptr(weak_document_ptr),
      journal_entry(std::move(journal_entry)) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
}
ToolController::ActiveState::~ActiveState() = default;

ToolController::ToolController() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolController::~ToolController() = default;

std::unique_ptr<Tool> ToolController::CreateTool(AggregatedJournal& journal,
                                                 TaskId task_id,
                                                 TabInterface* tab,
                                                 RenderFrameHost* frame,
                                                 const Action& action) {
  switch (action.action_case()) {
    case Action::kClick:
    case Action::kType:
    case Action::kScroll:
    case Action::kMoveMouse:
    case Action::kDragAndRelease:
    case Action::kSelect: {
      // PageTools are all implemented in the renderer so share the PageTool
      // implementation to shuttle them there.
      return std::make_unique<PageTool>(journal, *frame, action);
    }
    case Action::kNavigate: {
      GURL url(action.navigate().url());
      return std::make_unique<NavigateTool>(*tab->GetContents(), url);
    }
    case Action::kBack: {
      return std::make_unique<HistoryTool>(*tab->GetContents(),
                                           HistoryTool::kBack);
    }
    case Action::kForward: {
      return std::make_unique<HistoryTool>(*tab->GetContents(),
                                           HistoryTool::kForward);
    }
    case Action::kWait: {
      return std::make_unique<WaitTool>();
    }
    case Action::kCreateTab:
    case Action::kCloseTab:
    case Action::kActivateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ACTION_NOT_SET:
      NOTREACHED();
  }
}

void ToolController::Invoke(const Action& action,
                            AggregatedJournal& journal,
                            TaskId task_id,
                            tabs::TabInterface* tab,
                            content::RenderFrameHost* target_frame,
                            ResultCallback result_callback) {
  std::unique_ptr<Tool> created_tool =
      CreateTool(journal, task_id, tab, target_frame, action);

  if (!created_tool) {
    // Tool not found.
    PostResponseTask(std::move(result_callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown));
    return;
  }

  std::string url_spec;
  if (target_frame) {
    url_spec = target_frame->GetLastCommittedURL().possibly_invalid_spec();
  } else if (tab) {
    url_spec =
        tab->GetContents()->GetLastCommittedURL().possibly_invalid_spec();
  }

  auto journal_event = journal.CreatePendingAsyncEntry(
      url_spec, task_id, created_tool->JournalEvent(),
      created_tool->DebugString());

  content::WeakDocumentPtr document_ptr;
  if (target_frame) {
    document_ptr = target_frame->GetWeakDocumentPtr();
  }
  active_state_.emplace(std::move(created_tool), std::move(result_callback),
                        document_ptr, std::move(journal_event));

  active_state_->tool->Validate(base::BindOnce(
      &ToolController::ValidationComplete, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::ValidationComplete(mojom::ActionResultPtr result) {
  CHECK(active_state_);

  if (!IsOk(*result)) {
    CompleteToolRequest(std::move(result));
    return;
  }

  // TODO(crbug.com/389739308): Ensure the acting tab remains valid (i.e. alive
  // and focused), return error otherwise.

  RenderFrameHost* target_frame =
      active_state_->weak_document_ptr.AsRenderFrameHostIfValid();
  if (!target_frame) {
    CompleteToolRequest(MakeResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  observation_delayer_ =
      active_state_->tool->GetObservationDelayer(*target_frame);

  active_state_->tool->Invoke(base::BindOnce(
      &ToolController::DidFinishToolInvoke, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::DidFinishToolInvoke(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  if (observation_delayer_ && IsOk(*result)) {
    observation_delayer_->Wait(
        base::BindOnce(&ToolController::CompleteToolRequest,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  } else {
    CompleteToolRequest(std::move(result));
  }
}

void ToolController::CompleteToolRequest(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  active_state_->journal_entry->EndEntry(ToDebugString(*result));
  PostResponseTask(std::move(active_state_->completion_callback),
                   std::move(result));
  observation_delayer_.reset();
  active_state_.reset();
}

}  // namespace actor
