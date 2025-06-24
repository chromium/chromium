// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/bloom_filter.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

using ::content::RenderFrameHost;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::Coordinate;
using ::optimization_guide::proto::DragAndReleaseAction;
using ::optimization_guide::proto::MoveMouseAction;
using ::optimization_guide::proto::NavigateAction;
using ::optimization_guide::proto::ScrollAction;
using ::optimization_guide::proto::SelectAction;
using ::optimization_guide::proto::TypeAction;
using ::optimization_guide::proto::TypeAction_TypeMode;

BrowserAction MakeClick(RenderFrameHost& rfh, int content_node_id) {
  BrowserAction action;
  ClickAction* click = action.add_actions()->mutable_click();
  click->mutable_target()->set_content_node_id(content_node_id);
  click->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);
  return action;
}

BrowserAction MakeClick(const gfx::Point& click_point) {
  BrowserAction action;
  ClickAction* click = action.add_actions()->mutable_click();
  Coordinate* coordinate = click->mutable_target()->mutable_coordinate();
  coordinate->set_x(click_point.x());
  coordinate->set_y(click_point.y());
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);
  return action;
}

BrowserAction MakeHistoryBack() {
  BrowserAction action;
  action.add_actions()->mutable_back();
  return action;
}

BrowserAction MakeHistoryForward() {
  BrowserAction action;
  action.add_actions()->mutable_forward();
  return action;
}

BrowserAction MakeMouseMove(RenderFrameHost& rfh, int content_node_id) {
  BrowserAction action;
  MoveMouseAction* move = action.add_actions()->mutable_move_mouse();
  move->mutable_target()->set_content_node_id(content_node_id);
  move->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  return action;
}

BrowserAction MakeMouseMove(const gfx::Point& move_point) {
  BrowserAction action;
  MoveMouseAction* move = action.add_actions()->mutable_move_mouse();
  Coordinate* coordinate = move->mutable_target()->mutable_coordinate();
  coordinate->set_x(move_point.x());
  coordinate->set_y(move_point.y());
  return action;
}

BrowserAction MakeNavigate(std::string_view target_url) {
  BrowserAction action;
  NavigateAction* navigate = action.add_actions()->mutable_navigate();
  navigate->mutable_url()->assign(target_url);
  return action;
}

BrowserAction MakeType(RenderFrameHost& rfh,
                       int content_node_id,
                       std::string_view text,
                       bool follow_by_enter) {
  BrowserAction action;
  TypeAction* type_action = action.add_actions()->mutable_type();
  type_action->mutable_target()->set_content_node_id(content_node_id);
  type_action->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  type_action->set_text(text);
  // TODO(crbug.com/409570203): Tests should set a mode.
  type_action->set_mode(
      TypeAction_TypeMode::TypeAction_TypeMode_UNKNOWN_TYPE_MODE);
  type_action->set_follow_by_enter(follow_by_enter);
  return action;
}

BrowserAction MakeType(const gfx::Point& type_point,
                       std::string_view text,
                       bool follow_by_enter) {
  BrowserAction action;
  TypeAction* type_action = action.add_actions()->mutable_type();
  Coordinate* coordinate = type_action->mutable_target()->mutable_coordinate();
  coordinate->set_x(type_point.x());
  coordinate->set_y(type_point.y());
  type_action->set_text(text);
  // TODO(crbug.com/409570203): Tests should set a mode.
  type_action->set_mode(
      TypeAction_TypeMode::TypeAction_TypeMode_UNKNOWN_TYPE_MODE);
  type_action->set_follow_by_enter(follow_by_enter);
  return action;
}

BrowserAction MakeScroll(RenderFrameHost& rfh,
                         std::optional<int> content_node_id,
                         float scroll_offset_x,
                         float scroll_offset_y) {
  CHECK(!scroll_offset_x || !scroll_offset_y)
      << "Scroll action supports only one axis at a time.";
  BrowserAction action;
  ScrollAction* scroll = action.add_actions()->mutable_scroll();

  if (content_node_id.has_value()) {
    scroll->mutable_target()->set_content_node_id(content_node_id.value());
    scroll->mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token(
            *DocumentIdentifierUserData::GetDocumentIdentifier(
                rfh.GetGlobalFrameToken()));
  } else {
    CHECK(rfh.IsInPrimaryMainFrame())
        << "Empty target is only used to scroll the main frame";
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

BrowserAction MakeSelect(RenderFrameHost& rfh,
                         int content_node_id,
                         std::string_view value) {
  BrowserAction action;
  SelectAction* select_action = action.add_actions()->mutable_select();
  select_action->mutable_target()->set_content_node_id(content_node_id);
  select_action->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  select_action->set_value(value);
  return action;
}

BrowserAction MakeDragAndRelease(const gfx::Point& from_point,
                                 const gfx::Point& to_point) {
  BrowserAction action;
  DragAndReleaseAction* drag_and_release =
      action.add_actions()->mutable_drag_and_release();
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
  action.add_actions()->mutable_wait();
  return action;
}

void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr>& future) {
  const auto& result = *(future.Get());
  EXPECT_TRUE(IsOk(result))
      << "Expected OK result, got " << ToDebugString(result);
}

void ExpectErrorResult(base::test::TestFuture<mojom::ActionResultPtr>& future,
                       mojom::ActionResultCode expected_code) {
  const auto& result = *(future.Get());
  EXPECT_EQ(result.code, expected_code)
      << "Expected error " << base::to_underlying(expected_code) << ", got "
      << ToDebugString(result);
}

void SetUpBlocklist(base::CommandLine* command_line,
                    const std::string& blocked_host) {
  constexpr uint32_t kNumHashFunctions = 7;
  constexpr uint32_t kNumBits = 511;
  optimization_guide::BloomFilter blocklist_bloom_filter(kNumHashFunctions,
                                                         kNumBits);
  blocklist_bloom_filter.Add(blocked_host);
  std::string blocklist_bloom_filter_data(
      reinterpret_cast<const char*>(&blocklist_bloom_filter.bytes()[0]),
      blocklist_bloom_filter.bytes().size());

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blocklist_optimization_filter =
      config.add_optimization_blocklists();
  blocklist_optimization_filter->set_optimization_type(
      optimization_guide::proto::GLIC_ACTION_PAGE_BLOCK);
  blocklist_optimization_filter->mutable_bloom_filter()->set_num_hash_functions(
      kNumHashFunctions);
  blocklist_optimization_filter->mutable_bloom_filter()->set_num_bits(kNumBits);
  blocklist_optimization_filter->mutable_bloom_filter()->set_data(
      blocklist_bloom_filter_data);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  command_line->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
}

}  // namespace actor
