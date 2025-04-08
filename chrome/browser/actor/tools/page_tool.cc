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
using optimization_guide::proto::TypeAction_TypeMode;

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

// Set mojom for mouse move action based on proto. Returns false if the proto
// does not contain correct/sufficient information, true otherwise.
void SetMouseMoveToolArgs(actor::mojom::MouseMoveActionPtr& move,
                          ActionInformation action_info) {
  move->target = actor::mojom::ToolTarget::New(
      action_info.move_mouse().target().content_node_id());
}

// Set mojom for type action based on proto.
// Returns false if the proto does not contain correct/sufficient information,
// true otherwise.
bool SetTypeToolArgs(actor::mojom::TypeActionPtr& type_action,
                     const ActionInformation& action_info) {
  type_action->target = actor::mojom::ToolTarget::New(
      action_info.type().target().content_node_id());
  type_action->text = action_info.type().text();
  type_action->follow_by_enter = action_info.type().follow_by_enter();

  // Map proto enum to mojom enum
  switch (action_info.type().mode()) {
    case TypeAction_TypeMode::TypeAction_TypeMode_DELETE_EXISTING:
      type_action->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_PREPEND:
      type_action->mode = actor::mojom::TypeAction::Mode::kPrepend;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_APPEND:
      type_action->mode = actor::mojom::TypeAction::Mode::kAppend;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_UNKNOWN_TYPE_MODE:
    default:
      DLOG(ERROR) << "TypeAction proto type mode not supported"
                  << action_info.type().mode();
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
    case ActionInformation::ActionInfoCase::kType: {
      auto type = mojom::TypeAction::New();
      if (!SetTypeToolArgs(type, action_info)) {
        std::move(callback).Run(false);
        return;
      }
      request->action = mojom::ToolAction::NewType(std::move(type));
      break;
    }
    case ActionInformation::ActionInfoCase::kMoveMouse: {
      auto mouse_move = mojom::MouseMoveAction::New();
      SetMouseMoveToolArgs(mouse_move, action_info);
      request->action = mojom::ToolAction::NewMouseMove(std::move(mouse_move));
      break;
    }
    case ActionInformation::ActionInfoCase::kScroll:
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
