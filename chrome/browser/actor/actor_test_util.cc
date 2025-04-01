// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include "components/optimization_guide/proto/features/actions_data.pb.h"

using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;
using optimization_guide::proto::MoveMouseAction;

namespace actor {

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

}  // namespace actor
