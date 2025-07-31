// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/filters/bloom_filter.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

using ::content::RenderFrameHost;
using ::content::WebContents;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::Coordinate;
using ::optimization_guide::proto::CreateTabAction;
using ::optimization_guide::proto::DragAndReleaseAction;
using ::optimization_guide::proto::HistoryBackAction;
using ::optimization_guide::proto::HistoryForwardAction;
using ::optimization_guide::proto::MoveMouseAction;
using ::optimization_guide::proto::NavigateAction;
using ::optimization_guide::proto::ScrollAction;
using ::optimization_guide::proto::SelectAction;
using ::optimization_guide::proto::TypeAction;
using ::optimization_guide::proto::TypeAction_TypeMode;
using tabs::TabHandle;
using tabs::TabInterface;

Actions MakeClick(RenderFrameHost& rfh, int content_node_id) {
  Actions actions;
  ClickAction* click = actions.add_actions()->mutable_click();
  click->mutable_target()->set_content_node_id(content_node_id);
  click->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);

  auto* tab = TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&rfh));
  click->set_tab_id(tab->GetHandle().raw_value());

  return actions;
}

Actions MakeClick(TabHandle tab_handle, const gfx::Point& click_point) {
  Actions actions;
  ClickAction* click = actions.add_actions()->mutable_click();
  Coordinate* coordinate = click->mutable_target()->mutable_coordinate();
  coordinate->set_x(click_point.x());
  coordinate->set_y(click_point.y());
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);
  click->set_tab_id(tab_handle.raw_value());
  return actions;
}

Actions MakeHistoryBack(TabHandle tab_handle) {
  Actions actions;
  HistoryBackAction* back = actions.add_actions()->mutable_back();
  back->set_tab_id(tab_handle.raw_value());
  return actions;
}

Actions MakeHistoryForward(TabHandle tab_handle) {
  Actions actions;
  HistoryForwardAction* forward = actions.add_actions()->mutable_forward();
  forward->set_tab_id(tab_handle.raw_value());
  return actions;
}

Actions MakeMouseMove(RenderFrameHost& rfh, int content_node_id) {
  Actions actions;
  MoveMouseAction* move = actions.add_actions()->mutable_move_mouse();
  move->mutable_target()->set_content_node_id(content_node_id);
  move->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  return actions;
}

Actions MakeMouseMove(const gfx::Point& move_point) {
  Actions actions;
  MoveMouseAction* move = actions.add_actions()->mutable_move_mouse();
  Coordinate* coordinate = move->mutable_target()->mutable_coordinate();
  coordinate->set_x(move_point.x());
  coordinate->set_y(move_point.y());
  return actions;
}

Actions MakeNavigate(tabs::TabHandle tab_handle, std::string_view target_url) {
  Actions actions;
  NavigateAction* navigate = actions.add_actions()->mutable_navigate();
  navigate->mutable_url()->assign(target_url);
  navigate->set_tab_id(tab_handle.raw_value());
  return actions;
}

Actions MakeCreateTab(SessionID window_id, bool foreground) {
  Actions actions;
  CreateTabAction* create_tab = actions.add_actions()->mutable_create_tab();
  create_tab->set_foreground(foreground);
  create_tab->set_window_id(window_id.id());
  return actions;
}

Actions MakeType(RenderFrameHost& rfh,
                 int content_node_id,
                 std::string_view text,
                 bool follow_by_enter) {
  Actions actions;
  TypeAction* type_action = actions.add_actions()->mutable_type();
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
  return actions;
}

Actions MakeType(const gfx::Point& type_point,
                 std::string_view text,
                 bool follow_by_enter) {
  Actions actions;
  TypeAction* type_action = actions.add_actions()->mutable_type();
  Coordinate* coordinate = type_action->mutable_target()->mutable_coordinate();
  coordinate->set_x(type_point.x());
  coordinate->set_y(type_point.y());
  type_action->set_text(text);
  // TODO(crbug.com/409570203): Tests should set a mode.
  type_action->set_mode(
      TypeAction_TypeMode::TypeAction_TypeMode_UNKNOWN_TYPE_MODE);
  type_action->set_follow_by_enter(follow_by_enter);
  return actions;
}

Actions MakeScroll(RenderFrameHost& rfh,
                   std::optional<int> content_node_id,
                   float scroll_offset_x,
                   float scroll_offset_y) {
  CHECK(!scroll_offset_x || !scroll_offset_y)
      << "Scroll action supports only one axis at a time.";
  Actions actions;
  ScrollAction* scroll = actions.add_actions()->mutable_scroll();

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
  return actions;
}

Actions MakeSelect(RenderFrameHost& rfh,
                   int content_node_id,
                   std::string_view value) {
  Actions actions;
  SelectAction* select_action = actions.add_actions()->mutable_select();
  select_action->mutable_target()->set_content_node_id(content_node_id);
  select_action->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  select_action->set_value(value);
  return actions;
}

Actions MakeDragAndRelease(const gfx::Point& from_point,
                           const gfx::Point& to_point) {
  Actions actions;
  DragAndReleaseAction* drag_and_release =
      actions.add_actions()->mutable_drag_and_release();
  drag_and_release->mutable_from_target()->mutable_coordinate()->set_x(
      from_point.x());
  drag_and_release->mutable_from_target()->mutable_coordinate()->set_y(
      from_point.y());
  drag_and_release->mutable_to_target()->mutable_coordinate()->set_x(
      to_point.x());
  drag_and_release->mutable_to_target()->mutable_coordinate()->set_y(
      to_point.y());
  return actions;
}

Actions MakeWait() {
  Actions actions;
  actions.add_actions()->mutable_wait();
  return actions;
}

Actions MakeAttemptLogin() {
  Actions actions;
  actions.add_actions()->mutable_attempt_login();
  return actions;
}

TabHandle GetTab(content::RenderFrameHost& rfh) {
  auto* tab = TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&rfh));
  CHECK(tab);
  return tab->GetHandle();
}

Actions MakeScriptTool(content::RenderFrameHost& rfh,
                       const std::string& name,
                       const std::string& input_arguments) {
  Actions action;
  auto* script_tool = action.add_actions()->mutable_script_tool();
  script_tool->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  script_tool->set_tool_name(name);
  script_tool->set_input_arguments(input_arguments);

  script_tool->set_tab_id(GetTab(rfh).raw_value());

  return action;
}

PageTarget MakeTarget(content::RenderFrameHost& rfh, int content_node_id) {
  std::string document_identifier =
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken());
  return PageTarget(DomNode{.node_id = content_node_id,
                            .document_identifier = document_identifier});
}

PageTarget MakeTarget(const gfx::Point& point) {
  return PageTarget(point);
}

std::unique_ptr<ToolRequest> MakeClickRequest(content::RenderFrameHost& rfh,
                                              int content_node_id) {
  return std::make_unique<ClickToolRequest>(
      GetTab(rfh), MakeTarget(rfh, content_node_id), MouseClickType::kLeft,
      MouseClickCount::kSingle);
}

std::unique_ptr<ToolRequest> MakeClickRequest(TabInterface& tab,
                                              const gfx::Point& click_point) {
  return std::make_unique<ClickToolRequest>(
      tab.GetHandle(), MakeTarget(click_point), MouseClickType::kLeft,
      MouseClickCount::kSingle);
}

std::unique_ptr<ToolRequest> MakeHistoryBackRequest(TabInterface& tab) {
  return std::make_unique<HistoryToolRequest>(
      tab.GetHandle(), HistoryToolRequest::Direction::kBack);
}

std::unique_ptr<ToolRequest> MakeHistoryForwardRequest(TabInterface& tab) {
  return std::make_unique<HistoryToolRequest>(
      tab.GetHandle(), HistoryToolRequest::Direction::kForward);
}

std::unique_ptr<ToolRequest> MakeMouseMoveRequest(content::RenderFrameHost& rfh,
                                                  int content_node_id) {
  return std::make_unique<MoveMouseToolRequest>(
      GetTab(rfh), MakeTarget(rfh, content_node_id));
}

std::unique_ptr<ToolRequest> MakeMouseMoveRequest(
    TabInterface& tab,
    const gfx::Point& move_point) {
  return std::make_unique<MoveMouseToolRequest>(tab.GetHandle(),
                                                MakeTarget(move_point));
}
std::unique_ptr<ToolRequest> MakeNavigateRequest(TabInterface& tab,
                                                 std::string_view target_url) {
  return std::make_unique<NavigateToolRequest>(tab.GetHandle(),
                                               GURL(target_url));
}
std::unique_ptr<ToolRequest> MakeTypeRequest(content::RenderFrameHost& rfh,
                                             int content_node_id,
                                             std::string_view text,
                                             bool follow_by_enter) {
  // TODO(crbug.com/409570203): Tests should set a mode.
  return std::make_unique<TypeToolRequest>(
      GetTab(rfh), MakeTarget(rfh, content_node_id), text, follow_by_enter,
      TypeToolRequest::Mode::kReplace);
}
std::unique_ptr<ToolRequest> MakeTypeRequest(TabInterface& tab,
                                             const gfx::Point& type_point,
                                             std::string_view text,
                                             bool follow_by_enter) {
  return std::make_unique<TypeToolRequest>(
      tab.GetHandle(), MakeTarget(type_point), text, follow_by_enter,
      TypeToolRequest::Mode::kReplace);
}
std::unique_ptr<ToolRequest> MakeSelectRequest(content::RenderFrameHost& rfh,
                                               int content_node_id,
                                               std::string_view value) {
  return std::make_unique<SelectToolRequest>(
      GetTab(rfh), MakeTarget(rfh, content_node_id), value);
}

std::unique_ptr<ToolRequest> MakeScrollRequest(
    content::RenderFrameHost& rfh,
    std::optional<int> content_node_id,
    float scroll_offset_x,
    float scroll_offset_y) {
  CHECK(scroll_offset_x == 0.f || scroll_offset_y == 0.f);

  int node_id =
      content_node_id.has_value() ? *content_node_id : kRootElementDomNodeId;
  float distance = 0.f;
  ScrollToolRequest::Direction direction = ScrollToolRequest::Direction::kDown;

  if (scroll_offset_x > 0) {
    direction = ScrollToolRequest::Direction::kRight;
    distance = scroll_offset_x;
  } else if (scroll_offset_x < 0) {
    direction = ScrollToolRequest::Direction::kLeft;
    distance = -scroll_offset_x;
  }
  if (scroll_offset_y > 0) {
    direction = ScrollToolRequest::Direction::kDown;
    distance = scroll_offset_y;
  } else if (scroll_offset_y < 0) {
    direction = ScrollToolRequest::Direction::kUp;
    distance = -scroll_offset_y;
  }

  return std::make_unique<ScrollToolRequest>(
      GetTab(rfh), MakeTarget(rfh, node_id), direction, distance);
}
std::unique_ptr<ToolRequest> MakeDragAndReleaseRequest(
    TabInterface& tab,
    const gfx::Point& from_point,
    const gfx::Point& to_point) {
  return std::make_unique<DragAndReleaseToolRequest>(
      tab.GetHandle(), MakeTarget(from_point), MakeTarget(to_point));
}
std::unique_ptr<ToolRequest> MakeWaitRequest() {
  // TODO(bokan): Move this the default in WaitToolRequest.
  constexpr base::TimeDelta kWaitTime = base::Seconds(3);
  return std::make_unique<WaitToolRequest>(kWaitTime);
}

std::unique_ptr<ToolRequest> MakeCreateTabRequest(SessionID window_id,
                                                  bool foreground) {
  return std::make_unique<CreateTabToolRequest>(
      window_id.id(), foreground ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                 : WindowOpenDisposition::NEW_BACKGROUND_TAB);
}

std::unique_ptr<ToolRequest> MakeAttemptLoginRequest(TabInterface& tab) {
  return std::make_unique<AttemptLoginToolRequest>(tab.GetHandle());
}

std::unique_ptr<ToolRequest> MakeScriptToolRequest(
    content::RenderFrameHost& rfh,
    const std::string& name,
    const std::string& input_arguments) {
  return std::make_unique<ScriptToolRequest>(
      GetTab(rfh), MakeTarget(rfh, kRootElementDomNodeId), name,
      input_arguments);
}

std::vector<std::unique_ptr<ToolRequest>> ToRequestList(
    std::unique_ptr<ToolRequest> request) {
  std::vector<std::unique_ptr<ToolRequest>> vec;
  vec.push_back(std::move(request));
  return vec;
}

void ExpectOkResult(const mojom::ActionResult& result) {
  EXPECT_TRUE(IsOk(result))
      << "Expected OK result, got " << ToDebugString(result);
}

void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr>& future) {
  const auto& result = *(future.Get<0>());
  ExpectOkResult(result);
}

void ExpectOkResult(base::test::TestFuture<mojom::ActionResultPtr,
                                           std::optional<size_t>>& future) {
  const auto& result = *(future.Get<0>());
  ExpectOkResult(result);
}

void ExpectErrorResult(base::test::TestFuture<mojom::ActionResultPtr,
                                              std::optional<size_t>>& future,
                       mojom::ActionResultCode expected_code) {
  const auto& result = *(future.Get<0>());
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
