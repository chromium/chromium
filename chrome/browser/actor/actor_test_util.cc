// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include <string_view>

#include "base/values.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::DragAndReleaseAction;
using ::optimization_guide::proto::MoveMouseAction;
using ::optimization_guide::proto::NavigateAction;
using ::optimization_guide::proto::ScrollAction;
using ::optimization_guide::proto::SelectAction;
using ::optimization_guide::proto::TypeAction;
using ::optimization_guide::proto::TypeAction_TypeMode;

BrowserAction MakeClick(int content_node_id) {
  BrowserAction action;
  ClickAction* click = action.add_action_information()->mutable_click();
  click->mutable_target()->set_content_node_id(content_node_id);
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);
  return action;
}

BrowserAction MakeHistoryBack() {
  BrowserAction action;
  action.add_action_information()->mutable_back();
  return action;
}

BrowserAction MakeHistoryForward() {
  BrowserAction action;
  action.add_action_information()->mutable_forward();
  return action;
}

BrowserAction MakeMouseMove(int content_node_id) {
  BrowserAction action;
  MoveMouseAction* move = action.add_action_information()->mutable_move_mouse();
  move->mutable_target()->set_content_node_id(content_node_id);
  return action;
}

BrowserAction MakeNavigate(std::string_view target_url) {
  BrowserAction action;
  NavigateAction* navigate =
      action.add_action_information()->mutable_navigate();
  navigate->mutable_url()->assign(target_url);
  return action;
}

BrowserAction MakeType(int content_node_id,
                       std::string_view text,
                       bool follow_by_enter) {
  BrowserAction action;
  TypeAction* type_action = action.add_action_information()->mutable_type();
  type_action->mutable_target()->set_content_node_id(content_node_id);
  type_action->set_text(text);
  type_action->set_mode(TypeAction_TypeMode::TypeAction_TypeMode_APPEND);
  type_action->set_follow_by_enter(follow_by_enter);
  return action;
}

BrowserAction MakeScroll(std::optional<int> content_node_id,
                         float scroll_offset_x,
                         float scroll_offset_y) {
  CHECK(!scroll_offset_x || !scroll_offset_y)
      << "Scroll action supports only one axis at a time.";
  BrowserAction action;
  ScrollAction* scroll = action.add_action_information()->mutable_scroll();
  if (content_node_id.has_value()) {
    scroll->mutable_target()->set_content_node_id(content_node_id.value());
  }
  if (scroll_offset_x > 0) {
    scroll->set_direction(ScrollAction::RIGHT);
    scroll->set_distance(scroll_offset_x);
  } else if (scroll_offset_x < 0) {
    scroll->set_direction(ScrollAction::LEFT);
    scroll->set_distance(-scroll_offset_x);
  }
  if (scroll_offset_y > 0) {
    scroll->set_direction(ScrollAction::DOWN);
    scroll->set_distance(scroll_offset_y);
  } else if (scroll_offset_y < 0) {
    scroll->set_direction(ScrollAction::UP);
    scroll->set_distance(-scroll_offset_y);
  }
  return action;
}

BrowserAction MakeSelect(int content_node_id, std::string_view value) {
  BrowserAction action;
  SelectAction* select_action =
      action.add_action_information()->mutable_select();
  select_action->mutable_target()->set_content_node_id(content_node_id);
  select_action->set_value(value);
  return action;
}

BrowserAction MakeDragAndRelease(const gfx::Point& from_point,
                                 const gfx::Point& to_point) {
  BrowserAction action;
  DragAndReleaseAction* drag_and_release =
      action.add_action_information()->mutable_drag_and_release();
  drag_and_release->mutable_from_target()->mutable_coordinate()->set_x(
      from_point.x());
  drag_and_release->mutable_from_target()->mutable_coordinate()->set_y(
      from_point.y());
  drag_and_release->mutable_to_target()->mutable_coordinate()->set_x(
      to_point.x());
  drag_and_release->mutable_to_target()->mutable_coordinate()->set_y(
      to_point.y());
  return action;
}

BrowserAction MakeWait() {
  BrowserAction action;
  action.add_action_information()->mutable_wait();
  return action;
}

void OverrideActionObservationDelay(const base::TimeDelta& delta) {
  ActorCoordinator::SetActionObservationDelayForTesting(delta);
}

}  // namespace actor
