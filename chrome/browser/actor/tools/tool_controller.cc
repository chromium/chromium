// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/page_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using optimization_guide::proto::ActionInformation;
using tabs::TabInterface;

namespace actor {

ToolController::ActiveState::ActiveState(
    std::unique_ptr<Tool> tool,
    ToolInvocation::ResultCallback completion_callback)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
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
    case ActionInformation::kBack:
      // TODO(crbug.com/402730958): Implement
      NOTIMPLEMENTED();
      return nullptr;
    case ActionInformation::kForward:
      // TODO(crbug.com/402730309): Implement
      NOTIMPLEMENTED();
      return nullptr;
    case ActionInformation::kWait:
      // TODO(crbug.com/402730309): Implement
      NOTIMPLEMENTED();
      return nullptr;
    case ActionInformation::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }
}

void ToolController::Invoke(const ToolInvocation& invocation,
                            ToolInvocation::ResultCallback result_callback) {
  RenderFrameHost* target_frame = invocation.FindTargetFrame();
  if (!target_frame) {
    // The tab for this action was closed.
    PostResponseTask(std::move(result_callback), false);
    return;
  }

  std::unique_ptr<Tool> created_tool = CreateTool(*target_frame, invocation);

  if (!created_tool) {
    // Tool not found.
    PostResponseTask(std::move(result_callback), false);
    return;
  }

  active_state_.emplace(std::move(created_tool), std::move(result_callback));

  active_state_->tool->Validate(base::BindOnce(
      &ToolController::ValidationComplete, weak_ptr_factory_.GetWeakPtr()));
}

void ToolController::ValidationComplete(bool success) {
  if (!active_state_) {
    return;
  }

  // TODO(crbug.com/389739308): Provide more detail of failure to the caller.
  if (!success) {
    CompleteToolRequest(/*result=*/false);
    return;
  }

  // TODO(crbug.com/389739308): Ensure the acting tab remains valid (i.e. alive
  // and focused), return error otherwise.

  // Pass this by SafeRef since `this` owns the tool and any async behavior in
  // the tool should be tied to its lifetime.
  active_state_->tool->Invoke(base::BindOnce(
      &ToolController::CompleteToolRequest, weak_ptr_factory_.GetSafeRef()));
}

void ToolController::CompleteToolRequest(bool result) {
  CHECK(active_state_);
  PostResponseTask(std::move(active_state_->completion_callback), result);

  active_state_.reset();
}

}  // namespace actor
