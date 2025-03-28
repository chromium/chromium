// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::RenderFrameHost;
using optimization_guide::proto::ActionInformation;
using optimization_guide::proto::ClickAction_ClickCount;
using optimization_guide::proto::ClickAction_ClickType;

namespace {
// Set mojom for click action based on proto. Returns false if the proto does
// not contain correct/sufficient information, true otherwise.
bool SetClickToolArgs(actor::mojom::ClickActionPtr& click,
                      ActionInformation action_info) {
  click->target = actor::mojom::ToolTarget::New(
      action_info.click().target().content_node_id());
  switch (action_info.click().click_type()) {
    case ClickAction_ClickType::ClickAction_ClickType_LEFT:
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
    case ClickAction_ClickType::ClickAction_ClickType_RIGHT:
      click->type = actor::mojom::ClickAction::Type::kRight;
      break;
    default:
      return false;
  }
  switch (action_info.click().click_count()) {
    case ClickAction_ClickCount::ClickAction_ClickCount_SINGLE:
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
    case ClickAction_ClickCount::ClickAction_ClickCount_DOUBLE:
      click->count = actor::mojom::ClickAction::Count::kDouble;
      break;
    default:
      return false;
  }
  return true;
}
}  // namespace

namespace actor {

PageTool::PageTool(RenderFrameHost& frame, const ToolInvocation& invocation)
    : invocation_(invocation) {
  frame.GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame_);
}

PageTool::~PageTool() = default;

void PageTool::Validate(ValidateCallback callback) {
  // No browser-side validation yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void PageTool::Invoke(InvokeCallback callback) {
  auto request = actor::mojom::ToolInvocation::New();
  auto action_info = invocation_.GetActionInfo();

  switch (action_info.action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick: {
      auto click = mojom::ClickAction::New();
      if (!SetClickToolArgs(click, action_info)) {
        std::move(callback).Run(false);
        return;
      }
      request->action = mojom::ToolAction::NewClick(std::move(click));
      break;
    }
    case ActionInformation::ActionInfoCase::kType:
    case ActionInformation::ActionInfoCase::kScroll:
    case ActionInformation::ActionInfoCase::kMoveMouse:
    case ActionInformation::ActionInfoCase::kDragAndRelease:
    case ActionInformation::ActionInfoCase::kSelect: {
      // Not implemented yet.
      NOTIMPLEMENTED();
      std::move(callback).Run(false);
      return;
    }
    default:
      NOTREACHED();
  }

  chrome_render_frame_->InvokeTool(std::move(request), std::move(callback));
}

}  // namespace actor
