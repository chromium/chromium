// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/browser_action_util.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

// Alias the namespace to make the long enums a bit more readable in
// implementations.
namespace apc = ::optimization_guide::proto;

using apc::Action;
using apc::ActionTarget;
using apc::ActivateTabAction;
using apc::AttemptLoginAction;
using apc::ClickAction;
using apc::CloseTabAction;
using apc::CreateTabAction;
using apc::DragAndReleaseAction;
using apc::HistoryBackAction;
using apc::HistoryForwardAction;
using apc::MoveMouseAction;
using apc::NavigateAction;
using apc::ScriptToolAction;
using apc::ScrollAction;
using apc::SelectAction;
using apc::TypeAction;
using apc::WaitAction;
using ::optimization_guide::DocumentIdentifierUserData;
using ::page_content_annotations::FetchPageContextOptions;
using ::page_content_annotations::FetchPageContextResult;
using ::page_content_annotations::FetchPageContextResultCallbackArg;
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

std::optional<PageTarget> ToPageTarget(
    const optimization_guide::proto::ActionTarget& target) {
  // A valid target must have either a coordinate or a
  // document_identifier-dom_node_id pair.
  if (target.has_coordinate()) {
    return PageTarget(
        gfx::Point(target.coordinate().x(), target.coordinate().y()));
  } else {
    if (!target.has_content_node_id() || !target.has_document_identifier()) {
      return std::nullopt;
    }
    return PageTarget(
        DomNode{.node_id = target.content_node_id(),
                .document_identifier =
                    target.document_identifier().serialized_token()});
  }
}
std::unique_ptr<ToolRequest> CreateClickRequest(
    const ClickAction& action,
    TabInterface* deprecated_fallback_tab) {
  TabHandle tab_handle = GetTabHandle(action, deprecated_fallback_tab);

  if (!action.has_target() || !action.has_click_count() ||
      !action.has_click_type() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  MouseClickCount count;
  switch (action.click_count()) {
    case apc::ClickAction_ClickCount_SINGLE:
      count = MouseClickCount::kSingle;
      break;
    case apc::ClickAction_ClickCount_DOUBLE:
      count = MouseClickCount::kDouble;
      break;
    case apc::ClickAction_ClickCount_UNKNOWN_CLICK_COUNT:
    case apc::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(crbug.com/412700289): Revert once this is set.
      count = MouseClickCount::kSingle;
      break;
  }

  MouseClickType type;
  switch (action.click_type()) {
    case apc::ClickAction_ClickType_LEFT:
      type = MouseClickType::kLeft;
      break;
    case apc::ClickAction_ClickType_RIGHT:
      type = MouseClickType::kRight;
      break;
    case apc::
        ClickAction_ClickType_ClickAction_ClickType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case apc::
        ClickAction_ClickType_ClickAction_ClickType_INT_MAX_SENTINEL_DO_NOT_USE_:
    case apc::ClickAction_ClickType_UNKNOWN_CLICK_TYPE:
      // TODO(crbug.com/412700289): Revert once this is set.
      type = MouseClickType::kLeft;
      break;
  }

  auto target = ToPageTarget(action.target());
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

  auto target = ToPageTarget(action.target());
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

  std::optional<PageTarget> target;
  if (action.has_target()) {
    target = ToPageTarget(action.target());
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

    target.emplace(
        PageTarget(DomNode{.node_id = kRootElementDomNodeId,
                           .document_identifier = document_identifier}));
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

  auto target = ToPageTarget(action.target());
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

  auto from_target = ToPageTarget(action.from_target());
  if (!from_target.has_value()) {
    return nullptr;
  }

  auto to_target = ToPageTarget(action.to_target());
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

  auto target = ToPageTarget(action.target());
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

std::unique_ptr<ToolRequest> CreateAttemptLoginRequest(
    const AttemptLoginAction& action,
    TabInterface* deprecated_fallback_tab) {
  const tabs::TabHandle tab_handle =
      GetTabHandle(action, deprecated_fallback_tab);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  return std::make_unique<AttemptLoginToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateScriptToolRequest(
    const ScriptToolAction& action,
    TabInterface* deprecated_fallback_tab) {
  const tabs::TabHandle tab_handle =
      GetTabHandle(action, deprecated_fallback_tab);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  return std::make_unique<ScriptToolRequest>(
      tab_handle,
      PageTarget(DomNode{.node_id = kRootElementDomNodeId,
                         .document_identifier =
                             action.document_identifier().serialized_token()}),
      action.tool_name(), action.input_arguments());
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
    case optimization_guide::proto::Action::kAttemptLogin: {
      const AttemptLoginAction& attempt_login_action = action.attempt_login();
      return CreateAttemptLoginRequest(attempt_login_action,
                                       deprecated_fallback_tab);
    }
    case optimization_guide::proto::Action::kScriptTool: {
      const ScriptToolAction& script_tool_action = action.script_tool();
      return CreateScriptToolRequest(script_tool_action,
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
    default:
      NOTIMPLEMENTED();
      break;
  }

  return nullptr;
}

base::expected<std::vector<std::unique_ptr<ToolRequest>>, size_t>
BuildToolRequest(const optimization_guide::proto::Actions& actions) {
  std::vector<std::unique_ptr<ToolRequest>> requests;
  requests.reserve(actions.actions_size());
  for (int i = 0; i < actions.actions_size(); ++i) {
    std::unique_ptr<actor::ToolRequest> request = actor::CreateToolRequest(
        actions.actions().at(i), /*deprecated_fallback_tab=*/nullptr);
    if (request) {
      requests.push_back(std::move(request));
    } else {
      return base::unexpected(base::checked_cast<size_t>(i));
    }
  }

  return requests;
}

apc::TabObservation ConvertToTabObservation(
    const page_content_annotations::FetchPageContextResult& fetch_result) {
  apc::TabObservation tab_observation;

  if (fetch_result.screenshot_result) {
    auto& data = fetch_result.screenshot_result->jpeg_data;
    if (data.size() != 0) {
      tab_observation.set_screenshot_mime_type(kMimeTypeJpeg);
      // TODO(bokan): Can we avoid a copy here?
      tab_observation.set_screenshot(data.data(), data.size());
    }
  }

  if (fetch_result.annotated_page_content_result) {
    *tab_observation.mutable_annotated_page_content() =
        fetch_result.annotated_page_content_result->proto;
  }

  return tab_observation;
}

namespace {

void FetchCallback(base::RepeatingClosure barrier,
                   apc::TabObservation* tab_observation,
                   ActorKeyedService::TabObservationResult result) {
  base::ScopedClosureRunner run_barrier_at_return(barrier);

  if (!result.has_value()) {
    // TODO(crbug.com/435210098): There should be some way to message failure to
    // observe.
    return;
  }

  FetchPageContextResult& fetch_result = **result;

  // RequestTabObservation should return an error if these aren't filled in.
  CHECK(fetch_result.screenshot_result.has_value());
  CHECK(fetch_result.annotated_page_content_result.has_value());

  *tab_observation = ConvertToTabObservation(fetch_result);
}

}  // namespace

void BuildActionsResultWithObservations(
    content::BrowserContext& browser_context,
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    const ActorTask& task,
    base::OnceCallback<void(std::unique_ptr<apc::ActionsResult>)> callback) {
  auto response = std::make_unique<apc::ActionsResult>();

  response->set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response->set_index_of_failed_action(*index_of_failed_action);
  }

  auto* profile = Profile::FromBrowserContext(&browser_context);

  std::vector<Browser*> browsers = chrome::FindAllTabbedBrowsersWithProfile(
      profile, /*ignore_closing_browsers=*/true);

  for (Browser* browser : browsers) {
    apc::WindowObservation* window_observation = response->add_windows();
    window_observation->set_id(browser->session_id().id());
    window_observation->set_active(browser->IsActive());

    if (tabs::TabInterface* tab = browser->GetActiveTabInterface()) {
      window_observation->set_activated_tab_id(tab->GetHandle().raw_value());
    }

    for (const tabs::TabInterface* tab : *browser->GetTabStripModel()) {
      window_observation->add_tab_ids(tab->GetHandle().raw_value());
    }
  }

  absl::flat_hash_set<tabs::TabInterface*> tabs_to_fetch;

  for (const tabs::TabHandle& handle : task.GetLastActedTabs()) {
    // Include a TabObservation entry for acted on tabs. If the tab no longer
    // exists or the fetch context failed, the observation will be empty.
    // TODO(crbug.com/392167142): Check for a crashed tab here.
    // TODO(crbug.com/434263095): We should probably avoid capturing
    // observations if an action fails with kUrlBlocked. That might be better
    // implemented by not putting the tab into the LastActedTabs set.
    TabInterface* tab = handle.Get();
    if (!tab) {
      // TODO(crbug.com/435210098): There should be some way to message failure
      // to capture an observation to the model (here and in FetchCallback). For
      // now we leave the observation empty.
      apc::TabObservation* tab_observation = response->add_tabs();
      tab_observation->set_id(handle.raw_value());
    } else {
      tabs_to_fetch.insert(tab);
    }
  }

  apc::ActionsResult* raw_response = response.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      tabs_to_fetch.size(),
      base::BindOnce(std::move(callback), std::move(response)));

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  CHECK(actor_service);

  for (const tabs::TabInterface* tab : tabs_to_fetch) {
    apc::TabObservation* tab_observation = raw_response->add_tabs();
    tab_observation->set_id(tab->GetHandle().raw_value());

    // tab_observation can be Unretained because the underlying APC is owned by
    // the barrier which is ref-counted.
    actor_service->RequestTabObservation(
        *tab, base::BindOnce(FetchCallback, barrier,
                             base::Unretained(tab_observation)));
  }
}

apc::ActionsResult BuildErrorActionsResult(
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) {
  apc::ActionsResult response;
  CHECK(!IsOk(result_code));

  response.set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response.set_index_of_failed_action(*index_of_failed_action);
  }

  return response;
}

base::expected<std::vector<std::unique_ptr<ToolRequest>>, size_t>
BuildToolRequest(const optimization_guide::proto::BrowserAction& actions,
                 tabs::TabInterface* deprecated_fallback_tab) {
  std::vector<std::unique_ptr<actor::ToolRequest>> requests;
  requests.reserve(actions.actions_size());
  for (int i = 0; i < actions.actions_size(); ++i) {
    std::unique_ptr<actor::ToolRequest> request = actor::CreateToolRequest(
        actions.actions().at(i), deprecated_fallback_tab);
    if (request) {
      requests.push_back(std::move(request));
    } else {
      return base::unexpected(base::checked_cast<size_t>(i));
    }
  }

  return requests;
}

optimization_guide::proto::BrowserActionResult BuildBrowserActionResult(
    mojom::ActionResultCode result_code,
    int32_t tab_id) {
  optimization_guide::proto::BrowserActionResult response;
  response.set_action_result(static_cast<int32_t>(result_code));
  response.set_tab_id(tab_id);
  return response;
}

std::string ToBase64(const optimization_guide::proto::BrowserAction& actions) {
  size_t size = actions.ByteSizeLong();
  std::vector<uint8_t> buffer(size);
  actions.SerializeToArray(buffer.data(), size);
  return base::Base64Encode(buffer);
}

std::string ToBase64(const optimization_guide::proto::Actions& actions) {
  size_t size = actions.ByteSizeLong();
  std::vector<uint8_t> buffer(size);
  actions.SerializeToArray(buffer.data(), size);
  return base::Base64Encode(buffer);
}

}  // namespace actor
