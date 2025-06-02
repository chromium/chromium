// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/page_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using optimization_guide::proto::ActionInformation;
using tabs::TabInterface;

namespace actor {

ToolController::ActiveState::ActiveState(
    std::unique_ptr<Tool> tool,
    ToolInvocation::ResultCallback completion_callback,
    content::WeakDocumentPtr weak_document_ptr)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)),
      weak_document_ptr(weak_document_ptr) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
  CHECK(this->weak_document_ptr.AsRenderFrameHostIfValid());
}
ToolController::ActiveState::~ActiveState() = default;

ToolController::ToolController() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolController::~ToolController() = default;

std::unique_ptr<Tool> ToolController::CreateTool(
    RenderFrameHost& frame,
    const ToolInvocation& invocation) {
  switch (invocation.GetActionInfo().action_info_case()) {
    case ActionInformation::kClick:
    case ActionInformation::kType:
    case ActionInformation::kScroll:
    case ActionInformation::kMoveMouse:
    case ActionInformation::kDragAndRelease:
    case ActionInformation::kSelect: {
      // PageTools are all implemented in the renderer so share the PageTool
      // implementation to shuttle them there.
      return std::make_unique<PageTool>(frame, invocation);
    }
    case ActionInformation::kNavigate: {
      TabInterface* tab = invocation.FindTargetTab();
      GURL url(invocation.GetActionInfo().navigate().url());
      return std::make_unique<NavigateTool>(*tab, url);
    }
    case ActionInformation::kBack: {
      TabInterface* tab = invocation.FindTargetTab();
      return std::make_unique<HistoryTool>(*tab, HistoryTool::kBack);
    }
    case ActionInformation::kForward: {
      TabInterface* tab = invocation.FindTargetTab();
      return std::make_unique<HistoryTool>(*tab, HistoryTool::kForward);
    }
    case ActionInformation::kWait: {
      return std::make_unique<WaitTool>();
    }
    case ActionInformation::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }
}

void ToolController::Invoke(const ToolInvocation& invocation,
                            ToolInvocation::ResultCallback result_callback) {
  RenderFrameHost* target_frame = invocation.FindTargetFrame();
  if (!target_frame) {
    // The tab for this action was closed.
    PostResponseTask(std::move(result_callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  std::unique_ptr<Tool> created_tool = CreateTool(*target_frame, invocation);

  if (!created_tool) {
    // Tool not found.
    PostResponseTask(std::move(result_callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown));
    return;
  }

  ACTOR_LOG() << "Starting Tool Use: " << created_tool->DebugString();
  active_state_.emplace(std::move(created_tool), std::move(result_callback),
                        target_frame->GetWeakDocumentPtr());

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

  observation_delayer_.emplace(*target_frame,
                               active_state_->tool->GetObservationDelayType());

  active_state_->tool->Invoke(base::BindOnce(
      &ToolController::DidFinishToolInvoke, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::DidFinishToolInvoke(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  observation_delayer_->Wait(
      base::BindOnce(&ToolController::CompleteToolRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result)));
}

void ToolController::CompleteToolRequest(mojom::ActionResultPtr result) {
  CHECK(active_state_);
  ACTOR_LOG() << "Completed Tool Invoke[" << ToDebugString(*result)
              << "]: " << active_state_->tool->DebugString();
  PostResponseTask(std::move(active_state_->completion_callback),
                   std::move(result));
  observation_delayer_.reset();
  active_state_.reset();
}

}  // namespace actor
