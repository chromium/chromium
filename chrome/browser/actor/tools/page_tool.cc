// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/point.h"

namespace {

using ::content::RenderFrameHost;
using ::optimization_guide::proto::ActionInformation;
using ::optimization_guide::proto::ActionTarget;
using ::optimization_guide::proto::ClickAction_ClickCount;
using ::optimization_guide::proto::ClickAction_ClickType;
using ::optimization_guide::proto::ScrollAction_ScrollDirection;
using ::optimization_guide::proto::TypeAction_TypeMode;

void SetMojoTarget(const ActionTarget& target,
                   actor::mojom::ToolTargetPtr& out_mojo_target) {
  if (target.has_coordinate()) {
    out_mojo_target = actor::mojom::ToolTarget::NewCoordinate(
        gfx::Point(target.coordinate().x(), target.coordinate().y()));
  } else {
    // A ContentNodeId of 0 indicates the viewport. The mojo message indicates
    // viewport by omitting a target.
    if (target.content_node_id() > 0) {
      out_mojo_target =
          actor::mojom::ToolTarget::NewDomNodeId(target.content_node_id());
    }
  }
}

// Set mojom for click action based on proto. Returns false if the proto does
// not contain correct/sufficient information, true otherwise.
bool SetClickToolArgs(actor::mojom::ClickActionPtr& click,
                      const ActionInformation& action_info) {
  SetMojoTarget(action_info.click().target(), click->target);

  switch (action_info.click().click_type()) {
    case ClickAction_ClickType::ClickAction_ClickType_LEFT:
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
    case ClickAction_ClickType::ClickAction_ClickType_RIGHT:
      click->type = actor::mojom::ClickAction::Type::kRight;
      break;
    case ClickAction_ClickType::ClickAction_ClickType_UNKNOWN_CLICK_TYPE:
    case ClickAction_ClickType::
        ClickAction_ClickType_ClickAction_ClickType_INT_MAX_SENTINEL_DO_NOT_USE_:
    case ClickAction_ClickType::
        ClickAction_ClickType_ClickAction_ClickType_INT_MIN_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
      // return false;
  }

  switch (action_info.click().click_count()) {
    case ClickAction_ClickCount::ClickAction_ClickCount_SINGLE:
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
    case ClickAction_ClickCount::ClickAction_ClickCount_DOUBLE:
      click->count = actor::mojom::ClickAction::Count::kDouble;
      break;
    case ClickAction_ClickCount::ClickAction_ClickCount_UNKNOWN_CLICK_COUNT:
    case ClickAction_ClickCount::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ClickAction_ClickCount::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
      // return false;
  }
  return true;
}

// Set mojom for mouse move action based on proto.
void SetMouseMoveToolArgs(actor::mojom::MouseMoveActionPtr& move,
                          const ActionInformation& action_info) {
  SetMojoTarget(action_info.move_mouse().target(), move->target);
}

// Set mojom for type action based on proto.
// Returns false if the proto does not contain correct/sufficient information,
// true otherwise.
bool SetTypeToolArgs(actor::mojom::TypeActionPtr& type_action,
                     const ActionInformation& action_info) {
  SetMojoTarget(action_info.type().target(), type_action->target);

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
    case TypeAction_TypeMode::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case TypeAction_TypeMode::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      type_action->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
      break;
      //      DLOG(ERROR) << "TypeAction proto type mode not supported"
      //                  << action_info.type().mode();
      //      return false;
  }

  return true;
}

bool SetScrollToolArgs(actor::mojom::ScrollActionPtr& scroll,
                       const ActionInformation& action_info) {
  if (action_info.scroll().has_target()) {
    SetMojoTarget(action_info.scroll().target(), scroll->target);
  }
  switch (action_info.scroll().direction()) {
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_LEFT:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kLeft;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_RIGHT:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kRight;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_UP:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kUp;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_DOWN:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kDown;
      break;
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_UNKNOWN_SCROLL_DIRECTION:
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kDown;
      break;
      // return false;
  }
  scroll->distance = action_info.scroll().distance();
  return true;
}

void SetSelectToolArgs(actor::mojom::SelectActionPtr& select,
                       const ActionInformation& action_info) {
  SetMojoTarget(action_info.select().target(), select->target);
  select->value = action_info.select().value();
}

void SetDragAndReleaseToolArgs(
    actor::mojom::DragAndReleaseActionPtr& drag_and_release,
    ActionInformation action_info) {
  SetMojoTarget(action_info.drag_and_release().from_target(),
                drag_and_release->from_target);
  SetMojoTarget(action_info.drag_and_release().to_target(),
                drag_and_release->to_target);
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
      FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
}

void PageTool::Invoke(InvokeCallback callback) {
  auto request = actor::mojom::ToolInvocation::New();
  auto action_info = invocation_.GetActionInfo();

  switch (action_info.action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick: {
      auto click = mojom::ClickAction::New();
      if (!SetClickToolArgs(click, action_info)) {
        std::move(callback).Run(
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
        return;
      }
      request->action = mojom::ToolAction::NewClick(std::move(click));
      break;
    }
    case ActionInformation::ActionInfoCase::kType: {
      auto type = mojom::TypeAction::New();
      if (!SetTypeToolArgs(type, action_info)) {
        std::move(callback).Run(
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
        return;
      }
      request->action = mojom::ToolAction::NewType(std::move(type));
      break;
    }
    case ActionInformation::ActionInfoCase::kScroll: {
      auto scroll = mojom::ScrollAction::New();
      if (!SetScrollToolArgs(scroll, action_info)) {
        std::move(callback).Run(
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
        return;
      }
      request->action = mojom::ToolAction::NewScroll(std::move(scroll));
      break;
    }
    case ActionInformation::ActionInfoCase::kMoveMouse: {
      auto mouse_move = mojom::MouseMoveAction::New();
      SetMouseMoveToolArgs(mouse_move, action_info);
      request->action = mojom::ToolAction::NewMouseMove(std::move(mouse_move));
      break;
    }
    case ActionInformation::ActionInfoCase::kDragAndRelease: {
      auto drag_and_release = mojom::DragAndReleaseAction::New();
      SetDragAndReleaseToolArgs(drag_and_release, action_info);
      request->action =
          mojom::ToolAction::NewDragAndRelease(std::move(drag_and_release));
      break;
    }
    case ActionInformation::ActionInfoCase::kSelect: {
      auto select = mojom::SelectAction::New();
      SetSelectToolArgs(select, action_info);
      request->action = mojom::ToolAction::NewSelect(std::move(select));
      break;
    }
    case ActionInformation::ActionInfoCase::kNavigate:
    case ActionInformation::ActionInfoCase::kBack:
    case ActionInformation::ActionInfoCase::kForward:
    case ActionInformation::ActionInfoCase::kWait:
    case ActionInformation::ActionInfoCase::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }

  chrome_render_frame_->InvokeTool(std::move(request), std::move(callback));
}

std::string PageTool::DebugString() const {
  std::string tool_type;
  // TODO(crbug.com/402210051): Add more details here about tool params.
  switch (invocation_.GetActionInfo().action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick: {
      tool_type = "Click";
      break;
    }
    case ActionInformation::ActionInfoCase::kType: {
      tool_type = "Type";
      break;
    }
    case ActionInformation::ActionInfoCase::kScroll: {
      tool_type = "Scroll";
      break;
    }
    case ActionInformation::ActionInfoCase::kMoveMouse: {
      tool_type = "MoveMouse";
      break;
    }
    case ActionInformation::ActionInfoCase::kDragAndRelease: {
      tool_type = "DragAndRelease";
      break;
    }
    case ActionInformation::ActionInfoCase::kSelect: {
      tool_type = "Select";
      break;
    }
    case ActionInformation::ActionInfoCase::kNavigate:
    case ActionInformation::ActionInfoCase::kBack:
    case ActionInformation::ActionInfoCase::kForward:
    case ActionInformation::ActionInfoCase::kWait:
    case ActionInformation::ActionInfoCase::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }

  return absl::StrFormat("PageTool:%s", tool_type.c_str());
}

}  // namespace actor
