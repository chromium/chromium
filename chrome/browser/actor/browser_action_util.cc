// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/browser_action_util.h"

#include <optional>

#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

// Alias the namespace to make the long enums a bit more readable in
// implementations.
namespace apc = ::optimization_guide::proto;

using apc::Action;
using apc::ActionTarget;
using apc::ActivateTabAction;
using apc::ClickAction;
using apc::CloseTabAction;
using apc::CreateTabAction;
using apc::DragAndReleaseAction;
using apc::HistoryBackAction;
using apc::HistoryForwardAction;
using apc::MoveMouseAction;
using apc::NavigateAction;
using apc::ScrollAction;
using apc::SelectAction;
using apc::TypeAction;
using apc::WaitAction;
using ::optimization_guide::DocumentIdentifierUserData;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

namespace {

struct PageScopedParams {
  std::string document_identifier;
  TabHandle tab_handle;
};

template <class T>
TabHandle GetTabHandle(const T& action, TabInterface* deprecated_fallback_tab) {
  tabs::TabHandle tab_handle;
  if (action.has_tab_id()) {
    tab_handle = TabHandle(action.tab_id());
  } else if (deprecated_fallback_tab) {
    tab_handle = deprecated_fallback_tab->GetHandle();
  }
  return tab_handle;
}

std::optional<PageToolRequest::Target> ToPageToolTarget(
    const optimization_guide::proto::ActionTarget& target) {
  // A valid target must have either a coordinate or a
  // document_identifier-dom_node_id pair.
  if (target.has_coordinate()) {
    return PageToolRequest::Target(
        gfx::Point(target.coordinate().x(), target.coordinate().y()));
  } else {
    if (!target.has_content_node_id() || !target.has_document_identifier()) {
      return std::nullopt;
    }
    return PageToolRequest::Target(
        {target.content_node_id(),
         target.document_identifier().serialized_token()});
  }
}
std::unique_ptr<ToolRequest> CreateClickRequest(
    const ClickAction& action,
    TabInterface* deprecated_fallback_tab) {
  using ClickCount = ClickToolRequest::ClickCount;
  using ClickType = ClickToolRequest::ClickType;

  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);

  if (!action.has_target() || !action.has_click_count() ||
      !action.has_click_type() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  ClickCount count;
  switch (action.click_count()) {
    case apc::ClickAction_ClickCount_SINGLE:
      count = ClickCount::kSingle;
      break;
    case apc::ClickAction_ClickCount_DOUBLE:
      count = ClickCount::kDouble;
      break;
    case apc::ClickAction_ClickCount_UNKNOWN_CLICK_COUNT:
    case apc::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(crbug.com/412700289): Revert once this is set.
      count = ClickCount::kSingle;
      break;
  }

  ClickType type;
  switch (action.click_type()) {
    case apc::ClickAction_ClickType_LEFT:
      type = ClickType::kLeft;
      break;
    case apc::ClickAction_ClickType_RIGHT:
      type = ClickType::kRight;
      break;
    case apc::
        ClickAction_ClickType_ClickAction_ClickType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        ClickAction_ClickType_ClickAction_ClickType_INT_MAX_SENTINEL_DO_NOT_USE_:
    case apc::ClickAction_ClickType_UNKNOWN_CLICK_TYPE:
      // TODO(crbug.com/412700289): Revert once this is set.
      type = ClickType::kLeft;
      break;
  }

  auto target = ToPageToolTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }

  return std::make_unique<ClickToolRequest>(tab_handle, target.value(), type,
                                            count);
}

std::unique_ptr<ToolRequest> CreateTypeRequest(
    const TypeAction& action,
    TabInterface* deprecated_fallback_tab) {
  using TypeMode = TypeToolRequest::Mode;

  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);

  if (!action.has_target() || !action.has_text() || !action.has_mode() ||
      !action.has_follow_by_enter() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  TypeMode mode;
  switch (action.mode()) {
    case apc::TypeAction_TypeMode_DELETE_EXISTING:
      mode = TypeMode::kReplace;
      break;
    case apc::TypeAction_TypeMode_PREPEND:
      mode = TypeMode::kPrepend;
      break;
    case apc::TypeAction_TypeMode_APPEND:
      mode = TypeMode::kAppend;
      break;
    case apc::TypeAction_TypeMode_UNKNOWN_TYPE_MODE:
    case apc::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(crbug.com/412700289): Revert once this is set.
      mode = TypeMode::kReplace;
      break;
  }

  auto target = ToPageToolTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }
  return std::make_unique<TypeToolRequest>(tab_handle, target.value(),
                                           action.text(),
                                           action.follow_by_enter(), mode);
}

std::unique_ptr<ToolRequest> CreateScrollRequest(
    const ScrollAction& action,
    TabInterface* deprecated_fallback_tab) {
  using Direction = ScrollToolRequest::Direction;

  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);

  if (!action.has_direction() || !action.has_distance() ||
      tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  std::optional<PageToolRequest::Target> target;
  if (action.has_target()) {
    target = ToPageToolTarget(action.target());
  } else {
    // Scroll action may omit a target which means "target the viewport".
    TabInterface* tab = tab_handle.Get();
    if (!tab) {
      return nullptr;
    }
    std::string document_identifier =
        DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
            tab->GetContents()->GetPrimaryMainFrame())
            ->serialized_token();

    target.emplace(PageToolRequest::Target(
        {/*dom_node_id=*/kRootElementDomNodeId, document_identifier}));
  }

  if (!target) {
    return nullptr;
  }

  Direction direction;
  switch (action.direction()) {
    case apc::ScrollAction_ScrollDirection_LEFT:
      direction = Direction::kLeft;
      break;
    case apc::ScrollAction_ScrollDirection_RIGHT:
      direction = Direction::kRight;
      break;
    case apc::ScrollAction_ScrollDirection_UP:
      direction = Direction::kUp;
      break;
    case apc::ScrollAction_ScrollDirection_DOWN:
      direction = Direction::kDown;
      break;
    case apc::ScrollAction_ScrollDirection_UNKNOWN_SCROLL_DIRECTION:
    case apc::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(crbug.com/412700289): Revert once this is set.
      direction = Direction::kDown;
      break;
  }

  return std::make_unique<ScrollToolRequest>(tab_handle, target.value(),
                                             direction, action.distance());
}

std::unique_ptr<ToolRequest> CreateMoveMouseRequest(
    const MoveMouseAction& action,
    TabInterface* deprecated_fallback_tab) {
  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);
  if (!action.has_target() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  auto target = ToPageToolTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }

  return std::make_unique<MoveMouseToolRequest>(tab_handle, target.value());
}

std::unique_ptr<ToolRequest> CreateDragAndReleaseRequest(
    const DragAndReleaseAction& action,
    TabInterface* deprecated_fallback_tab) {
  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);

  if (!action.has_from_target() || !action.has_to_target() ||
      tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  auto from_target = ToPageToolTarget(action.from_target());
  if (!from_target.has_value()) {
    return nullptr;
  }

  auto to_target = ToPageToolTarget(action.to_target());
  if (!to_target.has_value()) {
    return nullptr;
  }

  return std::make_unique<DragAndReleaseToolRequest>(
      tab_handle, from_target.value(), to_target.value());
}

std::unique_ptr<ToolRequest> CreateSelectRequest(
    const SelectAction& action,
    TabInterface* deprecated_fallback_tab) {
  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);
  if (!action.has_value() || !action.has_target() ||
      tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  auto target = ToPageToolTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }

  return std::make_unique<SelectToolRequest>(tab_handle, target.value(),
                                             action.value());
}

std::unique_ptr<ToolRequest> CreateNavigateRequest(
    const NavigateAction& action,
    TabInterface* deprecated_fallback_tab) {
  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);
  if (!action.has_url() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  return std::make_unique<NavigateToolRequest>(tab_handle, GURL(action.url()));
}

std::unique_ptr<ToolRequest> CreateCreateTabRequest(
    const CreateTabAction& action) {
  if (!action.has_window_id()) {
    return nullptr;
  }

  int32_t window_id = action.window_id();

  // TODO(bokan): Is the foreground bit always set? If not, should this return
  // an error or default to what? For now we default to foreground.
  WindowOpenDisposition disposition =
      !action.has_foreground() || action.foreground()
          ? WindowOpenDisposition::NEW_FOREGROUND_TAB
          : WindowOpenDisposition::NEW_BACKGROUND_TAB;

  return std::make_unique<CreateTabToolRequest>(window_id, disposition);
}

std::unique_ptr<ToolRequest> CreateActivateTabRequest(
    const ActivateTabAction& action,
    TabInterface* deprecated_fallback_tab) {
  tabs::TabHandle tab_handle;
  if (action.has_tab_id()) {
    tab_handle = tabs::TabHandle(action.tab_id());
  } else if (deprecated_fallback_tab) {
    tab_handle = deprecated_fallback_tab->GetHandle();
  } else {
    return nullptr;
  }

  return std::make_unique<ActivateTabToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateCloseTabRequest(
    const CloseTabAction& action,
    TabInterface* deprecated_fallback_tab) {
  tabs::TabHandle tab_handle;
  if (action.has_tab_id()) {
    tab_handle = tabs::TabHandle(action.tab_id());
  } else if (deprecated_fallback_tab) {
    tab_handle = deprecated_fallback_tab->GetHandle();
  } else {
    return nullptr;
  }

  return std::make_unique<CloseTabToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateBackRequest(
    const HistoryBackAction& action,
    TabInterface* deprecated_fallback_tab) {
  tabs::TabHandle tab_handle;
  if (action.has_tab_id()) {
    tab_handle = tabs::TabHandle(action.tab_id());
  } else if (deprecated_fallback_tab) {
    tab_handle = deprecated_fallback_tab->GetHandle();
  } else {
    return nullptr;
  }

  return std::make_unique<HistoryToolRequest>(
      tab_handle, HistoryToolRequest::Direction::kBack);
}

std::unique_ptr<ToolRequest> CreateForwardRequest(
    const HistoryForwardAction& action,
    TabInterface* deprecated_fallback_tab) {
  tabs::TabHandle tab_handle;
  if (action.has_tab_id()) {
    tab_handle = tabs::TabHandle(action.tab_id());
  } else if (deprecated_fallback_tab) {
    tab_handle = deprecated_fallback_tab->GetHandle();
  } else {
    return nullptr;
  }

  return std::make_unique<HistoryToolRequest>(
      tab_handle, HistoryToolRequest::Direction::kForward);
}

std::unique_ptr<ToolRequest> CreateWaitRequest(const WaitAction& action) {
  constexpr base::TimeDelta kWaitTime = base::Seconds(3);
  return std::make_unique<WaitToolRequest>(kWaitTime);
}

}  // namespace

std::unique_ptr<ToolRequest> CreateToolRequest(
    const optimization_guide::proto::Action& action,
    TabInterface* deprecated_fallback_tab) {
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kClick: {
      const ClickAction& click_action = action.click();
      return CreateClickRequest(click_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kType: {
      const TypeAction& type_action = action.type();
      return CreateTypeRequest(type_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kScroll: {
      const ScrollAction& scroll_action = action.scroll();
      return CreateScrollRequest(scroll_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kMoveMouse: {
      const MoveMouseAction& move_mouse_action = action.move_mouse();
      return CreateMoveMouseRequest(move_mouse_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kDragAndRelease: {
      const DragAndReleaseAction& drag_action = action.drag_and_release();
      return CreateDragAndReleaseRequest(drag_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kSelect: {
      const SelectAction& select_action = action.select();
      return CreateSelectRequest(select_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kNavigate: {
      const NavigateAction& navigate_action = action.navigate();
      return CreateNavigateRequest(navigate_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kBack: {
      const HistoryBackAction& back_action = action.back();
      return CreateBackRequest(back_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kForward: {
      const HistoryForwardAction& forward_action = action.forward();
      return CreateForwardRequest(forward_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kWait: {
      const WaitAction& wait_action = action.wait();
      return CreateWaitRequest(wait_action);
    }
    case optimization_guide::proto::Action::kCreateTab: {
      const CreateTabAction& create_tab_action = action.create_tab();
      return CreateCreateTabRequest(create_tab_action);
    }
    case optimization_guide::proto::Action::kCloseTab: {
      const CloseTabAction& close_tab_action = action.close_tab();
      return CreateCloseTabRequest(close_tab_action, deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kActivateTab: {
      const ActivateTabAction& activate_tab_action = action.activate_tab();
      return CreateActivateTabRequest(activate_tab_action,
                                      deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kCreateWindow:
    case optimization_guide::proto::Action::kCloseWindow:
    case optimization_guide::proto::Action::kActivateWindow:
    case optimization_guide::proto::Action::kYieldToUser:
      NOTIMPLEMENTED();
      break;
    case optimization_guide::proto::Action::ACTION_NOT_SET:
      ACTOR_LOG() << "Action Type Not Set!";
      break;
  }

  return nullptr;
}

}  // namespace actor
