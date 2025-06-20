// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "url/gurl.h"

namespace actor {

using ::optimization_guide::proto::AnnotatedPageContent;

ToolController::ActiveState::ActiveState(
    std::unique_ptr<Tool> tool,
    ResultCallback completion_callback,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
    const optimization_guide::proto::AnnotatedPageContent* last_observation)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)),
      journal_entry(std::move(journal_entry)),
      last_observation(last_observation) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
}
ToolController::ActiveState::~ActiveState() = default;

ToolController::ToolController(TaskId task_id, AggregatedJournal& journal)
    : task_id_(task_id), journal_(journal.GetSafeRef()) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolController::~ToolController() = default;

void ToolController::Invoke(const ToolRequest& request,
                            const AnnotatedPageContent* last_observation,
                            ResultCallback result_callback) {
  ToolRequest::CreateToolResult create_result =
      request.CreateTool(task_id_, *journal_);

  if (!IsOk(*create_result.result)) {
    CHECK(!create_result.tool);
    journal_->Log(request.GetURLForJournal(), task_id_,
                  "ToolController Invoke Failed",
                  create_result.result->message);
    PostResponseTask(std::move(result_callback),
                     std::move(create_result.result));
    return;
  }

  std::unique_ptr<Tool>& tool = create_result.tool;
  CHECK(tool);

  auto journal_event = journal_->CreatePendingAsyncEntry(
      tool->JournalURL(), task_id_, tool->JournalEvent(), tool->DebugString());
  active_state_.emplace(std::move(tool), std::move(result_callback),
                        std::move(journal_event), last_observation);

  active_state_->tool->Validate(base::BindOnce(
      &ToolController::ValidationComplete, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::ValidationComplete(mojom::ActionResultPtr result) {
  CHECK(active_state_);

  if (!IsOk(*result)) {
    CompleteToolRequest(std::move(result));
    return;
  }

  mojom::ActionResultPtr toctou_result =
      active_state_->tool->TimeOfUseValidation(active_state_->last_observation);
  if (!IsOk(*toctou_result)) {
    CompleteToolRequest(std::move(toctou_result));
    return;
  }

  // TODO(crbug.com/389739308): Ensure the acting tab remains valid (i.e. alive
  // and focused), return error otherwise.

  observation_delayer_ = active_state_->tool->GetObservationDelayer();

  active_state_->tool->Invoke(base::BindOnce(
      &ToolController::DidFinishToolInvoke, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::DidFinishToolInvoke(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  if (observation_delayer_ && IsOk(*result)) {
    observation_delayer_->Wait(
        *active_state_->journal_entry,
        base::BindOnce(&ToolController::CompleteToolRequest,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  } else {
    CompleteToolRequest(std::move(result));
  }
}

void ToolController::CompleteToolRequest(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  observation_delayer_.reset();
  active_state_->journal_entry->EndEntry(ToDebugString(*result));
  PostResponseTask(std::move(active_state_->completion_callback),
                   std::move(result));
  active_state_.reset();
}

}  // namespace actor
