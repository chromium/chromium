// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/task_id.h"
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
#include "url/url_util.h"

namespace actor {

using ::content::RenderFrameHost;
using ::content::WebContents;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActivateWindowAction;
using ::optimization_guide::proto::ClickAction;
using ClickType = ::optimization_guide::proto::ClickAction::ClickType;
using ClickCount = ::optimization_guide::proto::ClickAction::ClickCount;
using ::optimization_guide::proto::CloseWindowAction;
using ::optimization_guide::proto::Coordinate;
using ::optimization_guide::proto::CreateTabAction;
using ::optimization_guide::proto::CreateWindowAction;
using ::optimization_guide::proto::DragAndReleaseAction;
using ::optimization_guide::proto::HistoryBackAction;
using ::optimization_guide::proto::HistoryForwardAction;
using ::optimization_guide::proto::MoveMouseAction;
using ::optimization_guide::proto::NavigateAction;
using ::optimization_guide::proto::ScrollAction;
using ::optimization_guide::proto::ScrollToAction;
using ::optimization_guide::proto::SelectAction;
using ::optimization_guide::proto::TypeAction;
using ::optimization_guide::proto::WaitAction;
using tabs::TabHandle;
using tabs::TabInterface;

namespace {
TabHandle GetTabHandleForFrame(content::RenderFrameHost& rfh) {
  auto* tab = TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&rfh));
  CHECK(tab);
  return tab->GetHandle();
}
}  // namespace

Actions MakeClick(RenderFrameHost& rfh,
                  int content_node_id,
                  ClickType click_type,
                  ClickCount click_count) {
  Actions actions;
  ClickAction* click = actions.add_actions()->mutable_click();
  click->mutable_target()->set_content_node_id(content_node_id);
  click->mutable_target()->mutable_document_identifier()->set_serialized_token(
      *DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  click->set_click_type(click_type);
  click->set_click_count(click_count);
  click->set_tab_id(GetTabHandleForFrame(rfh).raw_value());
  return actions;
}

Actions MakeClick(TabHandle tab_handle,
                  const gfx::Point& click_point,
                  ClickType click_type,
                  ClickCount click_count) {
  Actions actions;
  ClickAction* click = actions.add_actions()->mutable_click();
  Coordinate* coordinate = click->mutable_target()->mutable_coordinate();
  coordinate->set_x(click_point.x());
  coordinate->set_y(click_point.y());
  click->set_click_type(click_type);
  click->set_click_count(click_count);
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
  move->set_tab_id(GetTabHandleForFrame(rfh).raw_value());
  return actions;
}

Actions MakeMouseMove(TabHandle tab_handle, const gfx::Point& move_point) {
  Actions actions;
  MoveMouseAction* move = actions.add_actions()->mutable_move_mouse();
  Coordinate* coordinate = move->mutable_target()->mutable_coordinate();
  coordinate->set_x(move_point.x());
  coordinate->set_y(move_point.y());
  move->set_tab_id(tab_handle.raw_value());
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

Actions MakeActivateWindow(SessionID window_id) {
  Actions actions;
  ActivateWindowAction* activate_window =
      actions.add_actions()->mutable_activate_window();
  activate_window->set_window_id(window_id.id());
  return actions;
}

Actions MakeCreateWindow() {
  Actions actions;
  actions.add_actions()->mutable_create_window();
  return actions;
}

Actions MakeCloseWindow(SessionID window_id) {
  Actions actions;
  CloseWindowAction* close_window =
      actions.add_actions()->mutable_close_window();
  close_window->set_window_id(window_id.id());
  return actions;
}

Actions MakeType(RenderFrameHost& rfh,
                 int content_node_id,
                 std::string_view text,
                 bool follow_by_enter,
                 optimization_guide::proto::TypeAction::TypeMode mode) {
  // TODO(crbug.com/417270084): TypeAction currently only supports the
  // DELETE_EXISTING mode.
  CHECK_EQ(mode, optimization_guide::proto::TypeAction::TypeMode::
                     TypeAction_TypeMode_DELETE_EXISTING);

  Actions actions;
  TypeAction* type_action = actions.add_actions()->mutable_type();
  type_action->mutable_target()->set_content_node_id(content_node_id);
  type_action->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  type_action->set_text(text);
  type_action->set_mode(mode);
  type_action->set_follow_by_enter(follow_by_enter);
  type_action->set_tab_id(GetTabHandleForFrame(rfh).raw_value());
  return actions;
}

Actions MakeType(TabHandle tab_handle,
                 const gfx::Point& type_point,
                 std::string_view text,
                 bool follow_by_enter,
                 optimization_guide::proto::TypeAction::TypeMode mode) {
  Actions actions;
  TypeAction* type_action = actions.add_actions()->mutable_type();
  Coordinate* coordinate = type_action->mutable_target()->mutable_coordinate();
  coordinate->set_x(type_point.x());
  coordinate->set_y(type_point.y());
  type_action->set_text(text);
  // TODO(crbug.com/409570203): Tests should set a mode.
  // Currently uses DELETE_EXISTING behavior in all cases.
  type_action->set_mode(mode);
  type_action->set_follow_by_enter(follow_by_enter);
  type_action->set_tab_id(tab_handle.raw_value());
  return actions;
}

ScrollAction* MakeScrollHelper(RenderFrameHost& rfh,
                               Actions& actions,
                               float scroll_offset_x,
                               float scroll_offset_y) {
  CHECK(!scroll_offset_x || !scroll_offset_y)
      << "Scroll action supports only one axis at a time.";

  ScrollAction* scroll = actions.add_actions()->mutable_scroll();
  auto* tab = TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&rfh));
  scroll->set_tab_id(tab->GetHandle().raw_value());

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

  return scroll;
}

Actions MakeScroll(RenderFrameHost& rfh,
                   std::optional<int> content_node_id,
                   float scroll_offset_x,
                   float scroll_offset_y) {
  Actions actions;
  ScrollAction* scroll =
      MakeScrollHelper(rfh, actions, scroll_offset_x, scroll_offset_y);

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

  return actions;
}

Actions MakeScroll(RenderFrameHost& rfh,
                   const gfx::Point& scroll_point,
                   float scroll_offset_x,
                   float scroll_offset_y) {
  Actions actions;
  ScrollAction* scroll =
      MakeScrollHelper(rfh, actions, scroll_offset_x, scroll_offset_y);

  Coordinate* coordinate = scroll->mutable_target()->mutable_coordinate();
  coordinate->set_x(scroll_point.x());
  coordinate->set_y(scroll_point.y());

  return actions;
}

Actions MakeScrollTo(RenderFrameHost& rfh, int content_node_id) {
  Actions actions;
  ScrollToAction* scroll_to = actions.add_actions()->mutable_scroll_to();
  auto* tab = TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&rfh));
  scroll_to->set_tab_id(tab->GetHandle().raw_value());
  scroll_to->mutable_target()->set_content_node_id(content_node_id);
  scroll_to->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
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
  select_action->set_tab_id(GetTabHandleForFrame(rfh).raw_value());
  return actions;
}

Actions MakeDragAndRelease(tabs::TabHandle tab_handle,
                           const gfx::Point& from_point,
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
  drag_and_release->set_tab_id(tab_handle.raw_value());
  return actions;
}

Actions MakeDragAndRelease(content::RenderFrameHost& rfh,
                           int from_node_id,
                           int to_node_id) {
  Actions actions;
  DragAndReleaseAction* drag_and_release =
      actions.add_actions()->mutable_drag_and_release();

  drag_and_release->mutable_from_target()->set_content_node_id(from_node_id);
  drag_and_release->mutable_from_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));

  drag_and_release->mutable_to_target()->set_content_node_id(to_node_id);
  drag_and_release->mutable_to_target()
      ->mutable_document_identifier()
      ->set_serialized_token(*DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));

  drag_and_release->set_tab_id(GetTabHandleForFrame(rfh).raw_value());
  return actions;
}

Actions MakeWait(std::optional<base::TimeDelta> duration,
                 std::optional<TabHandle> observe_tab_handle) {
  Actions actions;
  WaitAction* wait = actions.add_actions()->mutable_wait();
  if (observe_tab_handle.has_value()) {
    wait->set_observe_tab_id(observe_tab_handle->raw_value());
  }
  if (duration.has_value()) {
    wait->set_wait_time_ms(duration->InMilliseconds());
  }
  return actions;
}

Actions MakeAttemptLogin() {
  Actions actions;
  actions.add_actions()->mutable_attempt_login();
  return actions;
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

  script_tool->set_tab_id(GetTabHandleForFrame(rfh).raw_value());

  return action;
}

Actions MakeMediaControl(tabs::TabHandle tab_handle,
                         MediaControl media_control) {
  Actions action;
  auto* media_control_action = action.add_actions()->mutable_media_control();
  media_control_action->set_tab_id(tab_handle.raw_value());

  if (std::get_if<PlayMedia>(&media_control)) {
    media_control_action->mutable_play();
  } else if (std::get_if<PauseMedia>(&media_control)) {
    media_control_action->mutable_pause();
  } else if (const auto* seek = std::get_if<SeekMedia>(&media_control)) {
    media_control_action->mutable_seek()->set_seek_time_microseconds(
        seek->seek_time_microseconds);
  }
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
      GetTabHandleForFrame(rfh), MakeTarget(rfh, content_node_id),
      MouseClickType::kLeft, MouseClickCount::kSingle);
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
      GetTabHandleForFrame(rfh), MakeTarget(rfh, content_node_id));
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
      GetTabHandleForFrame(rfh), MakeTarget(rfh, content_node_id), text,
      follow_by_enter, TypeToolRequest::Mode::kReplace);
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
      GetTabHandleForFrame(rfh), MakeTarget(rfh, content_node_id), value);
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
      GetTabHandleForFrame(rfh), MakeTarget(rfh, node_id), direction, distance);
}

std::unique_ptr<ToolRequest> MakeScrollToRequest(content::RenderFrameHost& rfh,
                                                 int content_node_id) {
  return std::make_unique<ScrollToToolRequest>(
      GetTabHandleForFrame(rfh), MakeTarget(rfh, content_node_id));
}

std::unique_ptr<ToolRequest> MakeDragAndReleaseRequest(
    TabInterface& tab,
    const gfx::Point& from_point,
    const gfx::Point& to_point) {
  return std::make_unique<DragAndReleaseToolRequest>(
      tab.GetHandle(), MakeTarget(from_point), MakeTarget(to_point));
}
std::unique_ptr<ToolRequest> MakeWaitRequest(TabInterface* observe_tab) {
  // TODO(bokan): Move this the default in WaitToolRequest.
  constexpr base::TimeDelta kWaitTime = base::Seconds(3);
  return std::make_unique<WaitToolRequest>(
      kWaitTime, observe_tab ? observe_tab->GetHandle() : TabHandle::Null());
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
      GetTabHandleForFrame(rfh),
      DomNode{.node_id = kRootElementDomNodeId,
              .document_identifier =
                  *DocumentIdentifierUserData::GetDocumentIdentifier(
                      rfh.GetGlobalFrameToken())},
      name, input_arguments);
}

std::unique_ptr<ToolRequest> MakeMediaControlRequest(
    tabs::TabInterface& tab,
    MediaControl media_control) {
  return std::make_unique<MediaControlToolRequest>(tab.GetHandle(),
                                                   media_control);
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

void ExpectOkResult(ActResultFuture& future) {
  const auto& result = *(future.Get<0>());
  ExpectOkResult(result);
}

void ExpectOkResult(PerformActionsFuture& future) {
  const auto& result = future.Get<0>();
  EXPECT_TRUE(IsOk(result)) << "Expected OK result, got " << result;
}

void ExpectErrorResult(ActResultFuture& future,
                       mojom::ActionResultCode expected_code) {
  const auto& result = *(future.Get<0>());
  EXPECT_EQ(result.code, expected_code)
      << "Result is " << ToDebugString(result);
}

void ExpectErrorResult(PerformActionsFuture& future,
                       mojom::ActionResultCode expected_code) {
  const auto& actual_code = future.Get<0>();
  EXPECT_EQ(actual_code, expected_code);
}

void PrintTo(const mojom::ActionResultCode& code, std::ostream* os) {
  *os << base::to_underlying(code);
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

std::string EncodeURI(const std::string& component) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(component, &encoded);
  return std::string(encoded.view());
}

}  // namespace actor
