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
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
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
using apc::ActivateWindowAction;
using apc::AttemptFormFillingAction;
using apc::AttemptLoginAction;
using apc::ClickAction;
using apc::CloseTabAction;
using apc::CloseWindowAction;
using apc::CreateTabAction;
using apc::CreateWindowAction;
using apc::DragAndReleaseAction;
using apc::HistoryBackAction;
using apc::HistoryForwardAction;
using apc::MediaControlAction;
using apc::MoveMouseAction;
using apc::NavigateAction;
using apc::ScriptToolAction;
using apc::ScrollAction;
using apc::ScrollToAction;
using apc::SelectAction;
using apc::TypeAction;
using apc::WaitAction;
using ::optimization_guide::DocumentIdentifierUserData;
using ::page_content_annotations::FetchPageContextError;
using ::page_content_annotations::FetchPageContextOptions;
using ::page_content_annotations::FetchPageContextResult;
using ::page_content_annotations::FetchPageContextResultCallbackArg;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

namespace {

// Test only callback for overriding the TabObservationResult provided from
// BuildActionsResultWithObservations.
base::RepeatingCallback<apc::TabObservation::TabObservationResult()>&
GetTabObservationResultOverrideForTesting() {
  static base::NoDestructor<
      base::RepeatingCallback<apc::TabObservation::TabObservationResult()>>
      callback;
  return *callback;
}

struct PageScopedParams {
  std::string document_identifier;
  TabHandle tab_handle;
};

template <class T>
TabHandle GetTabHandle(const T& action) {
  if (!action.has_tab_id()) {
    return TabHandle::Null();
  }

  return TabHandle(action.tab_id());
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
std::unique_ptr<ToolRequest> CreateClickRequest(const ClickAction& action) {
  TabHandle tab_handle = GetTabHandle(action);

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

std::unique_ptr<ToolRequest> CreateTypeRequest(const TypeAction& action) {
  using TypeMode = TypeToolRequest::Mode;

  TabHandle tab_handle = GetTabHandle(action);

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

std::unique_ptr<ToolRequest> CreateScrollRequest(const ScrollAction& action) {
  using Direction = ScrollToolRequest::Direction;

  TabHandle tab_handle = GetTabHandle(action);

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
    const MoveMouseAction& action) {
  TabHandle tab_handle = GetTabHandle(action);
  if (!action.has_target() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  auto target = ToPageTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }

  return std::make_unique<MoveMouseToolRequest>(tab_handle, target.value());
}

std::unique_ptr<ToolRequest> CreateScrollToRequest(
    const ScrollToAction& action) {
  TabHandle tab_handle = GetTabHandle(action);
  if (!action.has_target() || tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  auto target = ToPageTarget(action.target());
  if (!target.has_value()) {
    return nullptr;
  }

  return std::make_unique<ScrollToToolRequest>(tab_handle, target.value());
}

std::unique_ptr<ToolRequest> CreateDragAndReleaseRequest(
    const DragAndReleaseAction& action) {
  TabHandle tab_handle = GetTabHandle(action);

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

std::unique_ptr<ToolRequest> CreateSelectRequest(const SelectAction& action) {
  TabHandle tab_handle = GetTabHandle(action);
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
    const NavigateAction& action) {
  TabHandle tab_handle = GetTabHandle(action);
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
    const ActivateTabAction& action) {
  tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }
  return std::make_unique<ActivateTabToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateCloseTabRequest(
    const CloseTabAction& action) {
  tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }
  return std::make_unique<CloseTabToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateCreateWindowRequest(
    const CreateWindowAction& action) {
  return std::make_unique<CreateWindowToolRequest>();
}

std::unique_ptr<ToolRequest> CreateCloseWindowRequest(
    const CloseWindowAction& action) {
  if (!action.has_window_id()) {
    return nullptr;
  }

  return std::make_unique<CloseWindowToolRequest>(action.window_id());
}

std::unique_ptr<ToolRequest> CreateActivateWindowRequest(
    const ActivateWindowAction& action) {
  if (!action.has_window_id()) {
    return nullptr;
  }

  return std::make_unique<ActivateWindowToolRequest>(action.window_id());
}

std::unique_ptr<ToolRequest> CreateBackRequest(
    const HistoryBackAction& action) {
  tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }
  return std::make_unique<HistoryToolRequest>(
      tab_handle, HistoryToolRequest::Direction::kBack);
}

std::unique_ptr<ToolRequest> CreateForwardRequest(
    const HistoryForwardAction& action) {
  tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }
  return std::make_unique<HistoryToolRequest>(
      tab_handle, HistoryToolRequest::Direction::kForward);
}

std::unique_ptr<ToolRequest> CreateWaitRequest(const WaitAction& action) {
  const base::TimeDelta wait_time =
      action.has_wait_time_ms() ? base::Milliseconds(action.wait_time_ms())
                                : base::Seconds(3);
  const tabs::TabHandle observe_tab_handle =
      action.has_observe_tab_id() ? TabHandle(action.observe_tab_id())
                                  : TabHandle::Null();
  return std::make_unique<WaitToolRequest>(wait_time, observe_tab_handle);
}

std::unique_ptr<ToolRequest> CreateAttemptLoginRequest(
    const AttemptLoginAction& action) {
  const tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  return std::make_unique<AttemptLoginToolRequest>(tab_handle);
}

std::unique_ptr<ToolRequest> CreateAttemptFormFillingRequest(
    const AttemptFormFillingAction& action) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorAutofill)) {
    return nullptr;
  }

  const tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  if (action.form_filling_requests_size() == 0) {
    return nullptr;
  }

  auto requested_data_enum_converter = [](optimization_guide::proto::
                                              FormFillingRequest_RequestedData
                                                  proto_enum) {
    switch (proto_enum) {
      case optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS:
        return AttemptFormFillingToolRequest::RequestedData::kAddress;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_BILLING_ADDRESS:
        return AttemptFormFillingToolRequest::RequestedData::kBillingAddress;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_SHIPPING_ADDRESS:
        return AttemptFormFillingToolRequest::RequestedData::kShippingAddress;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_WORK_ADDRESS:
        return AttemptFormFillingToolRequest::RequestedData::kWorkAddress;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_HOME_ADDRESS:
        return AttemptFormFillingToolRequest::RequestedData::kHomeAddress;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_CREDIT_CARD:
        return AttemptFormFillingToolRequest::RequestedData::kCreditCard;
      case optimization_guide::proto::
          FormFillingRequest_RequestedData_CONTACT_INFORMATION:
        return AttemptFormFillingToolRequest::RequestedData::
            kContactInformation;
      default:
        // A default is needed:
        // 1. To ease importing the actions_data.proto from an external
        //    repository. Otherwise, the actions_data.proto import would be
        //    blocked by the implementation here.
        // 2. Since an old build may receive a yet unimported enum value in a
        //    new proto message.
        NOTIMPLEMENTED();
        return AttemptFormFillingToolRequest::RequestedData::kUnknown;
    }
  };

  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests;
  for (const auto& request_proto : action.form_filling_requests()) {
    AttemptFormFillingToolRequest::FormFillingRequest request;
    request.requested_data =
        requested_data_enum_converter(request_proto.requested_data());
    for (const auto& trigger_field : request_proto.trigger_fields()) {
      std::optional<PageTarget> page_target = ToPageTarget(trigger_field);
      if (!page_target) {
        // One of the targets is invalid.
        return nullptr;
      }
      request.trigger_fields.push_back(*page_target);
    }
    requests.push_back(request);
  }

  return std::make_unique<AttemptFormFillingToolRequest>(tab_handle,
                                                         std::move(requests));
}

std::unique_ptr<ToolRequest> CreateScriptToolRequest(
    const ScriptToolAction& action) {
  const tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  // TODO(khushalsagar): Remove once the callers are setting up this ID
  // correctly.
  std::string document_identifier;
  if (action.has_document_identifier()) {
    document_identifier = action.document_identifier().serialized_token();
  } else {
    auto* main_rfh = tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
    document_identifier = DocumentIdentifierUserData::GetDocumentIdentifier(
                              main_rfh->GetGlobalFrameToken())
                              .value_or("");
  }

  return std::make_unique<ScriptToolRequest>(
      tab_handle,
      DomNode{.node_id = kRootElementDomNodeId,
              .document_identifier = document_identifier},
      action.tool_name(), action.input_arguments());
}

std::unique_ptr<ToolRequest> CreateMediaControlRequest(
    const MediaControlAction& action) {
  const tabs::TabHandle tab_handle = GetTabHandle(action);
  if (tab_handle == TabHandle::Null()) {
    return nullptr;
  }

  MediaControl media_control;
  switch (action.media_control_action_case()) {
    case MediaControlAction::kPlay:
      media_control = PlayMedia();
      break;
    case MediaControlAction::kPause:
      media_control = PauseMedia();
      break;
    case MediaControlAction::kSeek:
      media_control = SeekMedia{.seek_time_microseconds =
                                    action.seek().seek_time_microseconds()};
      break;
    default:
      return nullptr;
  }

  return std::make_unique<MediaControlToolRequest>(tab_handle, media_control);
}

class ActorJournalFetchPageProgressListener
    : public page_content_annotations::FetchPageProgressListener {
 public:
  ActorJournalFetchPageProgressListener(
      base::SafeRef<AggregatedJournal> journal,
      const GURL& url,
      TaskId task_id)
      : journal_(journal), url_(url), task_id_(task_id) {}

  ~ActorJournalFetchPageProgressListener() override = default;

  void BeginScreenshot() override {
    screenshot_entry_ = journal_->CreatePendingAsyncEntry(
        url_, task_id_, journal_->AllocateDynamicTrackUUID(), "GrabScreenshot",
        {});
  }

  void EndScreenshot(std::optional<std::string> error) override {
    if (error.has_value()) {
      screenshot_entry_->EndEntry(
          JournalDetailsBuilder().AddError(*error).Build());
    } else {
      screenshot_entry_->EndEntry({});
    }
  }

  void BeginAPC() override {
    apc_entry_ = journal_->CreatePendingAsyncEntry(
        url_, task_id_, journal_->AllocateDynamicTrackUUID(), "GrabAPC", {});
  }

  void EndAPC(std::optional<std::string> error) override {
    if (error.has_value()) {
      apc_entry_->EndEntry(JournalDetailsBuilder().AddError(*error).Build());
    } else {
      apc_entry_->EndEntry({});
    }
  }

 private:
  base::SafeRef<AggregatedJournal> journal_;
  GURL url_;
  TaskId task_id_;
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> screenshot_entry_;
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> apc_entry_;
};

std::unique_ptr<ToolRequest> CreateToolRequest(
    const optimization_guide::proto::Action& action) {
  TRACE_EVENT1("actor", "CreateToolRequest", "action_type",
               static_cast<int>(action.action_case()));
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kClick: {
      const ClickAction& click_action = action.click();
      return CreateClickRequest(click_action);
    }
    case optimization_guide::proto::Action::kType: {
      const TypeAction& type_action = action.type();
      return CreateTypeRequest(type_action);
    }
    case optimization_guide::proto::Action::kScroll: {
      const ScrollAction& scroll_action = action.scroll();
      return CreateScrollRequest(scroll_action);
    }
    case optimization_guide::proto::Action::kMoveMouse: {
      const MoveMouseAction& move_mouse_action = action.move_mouse();
      return CreateMoveMouseRequest(move_mouse_action);
    }
    case optimization_guide::proto::Action::kDragAndRelease: {
      const DragAndReleaseAction& drag_action = action.drag_and_release();
      return CreateDragAndReleaseRequest(drag_action);
    }
    case optimization_guide::proto::Action::kSelect: {
      const SelectAction& select_action = action.select();
      return CreateSelectRequest(select_action);
    }
    case optimization_guide::proto::Action::kNavigate: {
      const NavigateAction& navigate_action = action.navigate();
      return CreateNavigateRequest(navigate_action);
    }
    case optimization_guide::proto::Action::kBack: {
      const HistoryBackAction& back_action = action.back();
      return CreateBackRequest(back_action);
    }
    case optimization_guide::proto::Action::kForward: {
      const HistoryForwardAction& forward_action = action.forward();
      return CreateForwardRequest(forward_action);
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
      return CreateCloseTabRequest(close_tab_action);
    }
    case optimization_guide::proto::Action::kActivateTab: {
      const ActivateTabAction& activate_tab_action = action.activate_tab();
      return CreateActivateTabRequest(activate_tab_action);
    }
    case optimization_guide::proto::Action::kAttemptLogin: {
      const AttemptLoginAction& attempt_login_action = action.attempt_login();
      return CreateAttemptLoginRequest(attempt_login_action);
    }
    case optimization_guide::proto::Action::kAttemptFormFilling: {
      const AttemptFormFillingAction& attempt_form_fill_action =
          action.attempt_form_filling();
      return CreateAttemptFormFillingRequest(attempt_form_fill_action);
    }
    case optimization_guide::proto::Action::kScriptTool: {
      const ScriptToolAction& script_tool_action = action.script_tool();
      return CreateScriptToolRequest(script_tool_action);
    }
    case optimization_guide::proto::Action::kScrollTo: {
      const ScrollToAction& scroll_to_action = action.scroll_to();
      return CreateScrollToRequest(scroll_to_action);
    }
    case optimization_guide::proto::Action::kMediaControl: {
      const MediaControlAction& media_control_action = action.media_control();
      return CreateMediaControlRequest(media_control_action);
    }
    case optimization_guide::proto::Action::kCreateWindow: {
      const CreateWindowAction& create_window_action = action.create_window();
      return CreateCreateWindowRequest(create_window_action);
    }
    case optimization_guide::proto::Action::kCloseWindow: {
      const CloseWindowAction& close_window_action = action.close_window();
      return CreateCloseWindowRequest(close_window_action);
    }
    case optimization_guide::proto::Action::kActivateWindow: {
      const ActivateWindowAction& activate_window_action =
          action.activate_window();
      return CreateActivateWindowRequest(activate_window_action);
    }
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

}  // namespace

base::expected<std::vector<std::unique_ptr<ToolRequest>>, size_t>
BuildToolRequest(const optimization_guide::proto::Actions& actions) {
  TRACE_EVENT0("actor", "BuildToolRequest");
  std::vector<std::unique_ptr<ToolRequest>> requests;
  requests.reserve(actions.actions_size());
  for (int i = 0; i < actions.actions_size(); ++i) {
    std::unique_ptr<ToolRequest> request =
        CreateToolRequest(actions.actions().at(i));
    if (request) {
      requests.push_back(std::move(request));
    } else {
      return base::unexpected(base::checked_cast<size_t>(i));
    }
  }

  return requests;
}

void FillInTabObservation(
    const page_content_annotations::FetchPageContextResult& fetch_result,
    apc::TabObservation& tab_observation) {
  TRACE_EVENT0("actor", "FillInTabObservation");
  if (fetch_result.screenshot_result.has_value()) {
    auto& data = fetch_result.screenshot_result->screenshot_data;
    if (data.size() != 0) {
      tab_observation.set_screenshot_mime_type(fetch_result.screenshot_result->mime_type);
      // TODO(bokan): Can we avoid a copy here?
      tab_observation.set_screenshot(data.data(), data.size());
    }
  }

  if (fetch_result.annotated_page_content_result.has_value()) {
    *tab_observation.mutable_annotated_page_content() =
        fetch_result.annotated_page_content_result->proto;
    if (fetch_result.annotated_page_content_result->metadata) {
      auto* proto_metadata = tab_observation.mutable_metadata();
      const auto& mojom_metadata =
          *fetch_result.annotated_page_content_result->metadata;
      for (const auto& mojom_frame_metadata : mojom_metadata.frame_metadata) {
        auto* proto_frame_metadata = proto_metadata->add_frame_metadata();
        proto_frame_metadata->set_url(mojom_frame_metadata->url.spec());
        for (const auto& mojom_meta_tag : mojom_frame_metadata->meta_tags) {
          auto* proto_meta_tag = proto_frame_metadata->add_meta_tags();
          proto_meta_tag->set_name(mojom_meta_tag->name);
          proto_meta_tag->set_content(mojom_meta_tag->content);
        }
      }
    }
  }
}

namespace {

apc::TabObservation::TabObservationResult ToTabObservationResult(
    FetchPageContextError error) {
  switch (error) {
    case FetchPageContextError::kUnknown:
      return apc::TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    case FetchPageContextError::kWebContentsChanged:
      return apc::TabObservation::TAB_OBSERVATION_WEB_CONTENTS_CHANGED;
    case FetchPageContextError::kPageContextNotEligible:
      return apc::TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE;
      ;
  }
}

void FetchCallback(
    TabHandle tab_handle,
    base::WeakPtr<Profile> profile,
    TaskId task_id,
    base::RepeatingClosure barrier,
    apc::TabObservation* tab_observation,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    base::TimeTicks start_time,
    base::TimeTicks fetch_context_time,
    apc::ActionsResult_LatencyInformation* latency_info,
    ActorKeyedService::TabObservationResult result) {
  TRACE_EVENT0("actor", "FetchCallback");
  CHECK(tab_observation);
  CHECK(latency_info);
  base::ScopedClosureRunner run_barrier_at_return(barrier);

  if (!profile) {
    return;
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile.get());
  TabInterface* const tab = tab_handle.Get();

  if (!GetTabObservationResultOverrideForTesting().is_null()) {
    tab_observation->set_result(
        GetTabObservationResultOverrideForTesting().Run());
    return;
  }

  if (!tab || !tab->GetContents()) {
    actor_service->GetJournal().Log(GURL(), task_id, "FetchCallback",
                                    JournalDetailsBuilder()
                                        .Add("tabId", tab_observation->id())
                                        .AddError("TabWentAway")
                                        .Build());
    tab_observation->set_result(
        apc::TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY);
    return;
  }

  if (tab->GetContents()->IsCrashed()) {
    actor_service->GetJournal().Log(GURL(), task_id, "FetchCallback",
                                    JournalDetailsBuilder()
                                        .Add("tabId", tab_observation->id())
                                        .AddError("Page crashed")
                                        .Build());
    tab_observation->set_result(
        apc::TabObservation::TAB_OBSERVATION_PAGE_CRASHED);
    return;
  }

  if (std::optional<std::string> error_message =
          ActorKeyedService::ExtractErrorMessageIfFailed(result)) {
    actor_service->GetJournal().Log(
        GURL(), task_id, *error_message,
        JournalDetailsBuilder().Add("tabId", tab_observation->id()).Build());
  }

  if (!result.has_value()) {
    tab_observation->set_result(
        ToTabObservationResult(result.error().error_code));
    return;
  }

  FetchPageContextResult& fetch_result = **result;

  bool has_apc = fetch_result.annotated_page_content_result.has_value();
  tab_observation->set_annotated_page_content_result(
      has_apc ? apc::TabObservation::ANNOTATED_PAGE_CONTENT_OK
              : apc::TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);

  bool has_screenshot = fetch_result.screenshot_result.has_value();
  tab_observation->set_screenshot_result(
      has_screenshot ? apc::TabObservation::SCREENSHOT_OK
                     : apc::TabObservation::SCREENSHOT_ERROR);

  // Context for actor observations should always have an APC and a screenshot,
  // return failure if either is missing.
  if (!has_apc || !has_screenshot) {
    tab_observation->set_result(
        apc::TabObservation::TAB_OBSERVATION_FETCH_ERROR);
    return;
  }

  tab_observation->set_result(apc::TabObservation::TAB_OBSERVATION_OK);
  {
    apc::ActionsResult_LatencyInformation_LatencyStep* latency_step =
        latency_info->add_latency_steps();
    latency_step->mutable_annotated_page_content()->set_id(
        tab_observation->id());
    latency_step->set_latency_start_ms(
        (fetch_context_time - start_time).InMilliseconds());
    latency_step->set_latency_stop_ms(
        (fetch_result.annotated_page_content_result.value().end_time -
         start_time)
            .InMilliseconds());
    base::UmaHistogramMediumTimes(
        "Actor.PageContext.APC.Duration",
        fetch_result.annotated_page_content_result.value().end_time -
            fetch_context_time);
  }

  {
    apc::ActionsResult_LatencyInformation_LatencyStep* latency_step =
        latency_info->add_latency_steps();
    latency_step->mutable_screenshot()->set_id(tab_observation->id());
    latency_step->set_latency_start_ms(
        (fetch_context_time - start_time).InMilliseconds());
    latency_step->set_latency_stop_ms(
        (fetch_result.screenshot_result.value().end_time - start_time)
            .InMilliseconds());
    base::UmaHistogramMediumTimes(
        "Actor.PageContext.Screenshot.Duration",
        fetch_result.screenshot_result.value().end_time - fetch_context_time);
  }

  // TODO(khushalsagar): Remove this once consumers use ActionResults for script
  // tool results.
  CopyScriptToolResults(*fetch_result.annotated_page_content_result->proto
                             .mutable_main_frame_data(),
                        action_results);

  FillInTabObservation(fetch_result, *tab_observation);
}

}  // namespace

void BuildActionsResultWithObservations(
    content::BrowserContext& browser_context,
    base::TimeTicks actions_start_time,
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    const ActorTask& task,
    bool skip_async_observation_information,
    base::OnceCallback<
        void(base::TimeTicks actions_start_time,
             mojom::ActionResultCode result_code,
             std::optional<size_t> index_of_failed_action,
             std::vector<actor::ActionResultWithLatencyInfo> action_results,
             actor::TaskId task_id,
             bool skip_async_observation_information,
             std::unique_ptr<apc::ActionsResult>,
             std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>)>
        callback) {
  TRACE_EVENT0("actor", "BuildActionsResultWithObservations");
  auto* profile = Profile::FromBrowserContext(&browser_context);
  auto* actor_service = actor::ActorKeyedService::Get(profile);
  CHECK(actor_service);

  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry =
      actor_service->GetJournal().CreatePendingAsyncEntry(
          GURL(), task.id(), MakeBrowserTrackUUID(task.id()),
          "BuildActionsResultWithObservations",
          JournalDetailsBuilder()
              .Add("result_code", base::ToString(result_code))
              .Add("index_of_failed_action",
                   index_of_failed_action.has_value()
                       ? base::ToString(*index_of_failed_action)
                       : std::string("<empty>"))
              .Add("skip_async_observation_information",
                   skip_async_observation_information)
              .Add("action_results", action_results.size())
              .Build());

  auto response = std::make_unique<apc::ActionsResult>();

  response->set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response->set_index_of_failed_action(*index_of_failed_action);
  }
  CopyScriptToolResults(*response, action_results);

  apc::ActionsResult_LatencyInformation* latency_info =
      response->mutable_latency_information();
  for (size_t i = 0; i < action_results.size(); ++i) {
    auto& action_result = action_results.at(i);
    CHECK(action_result.result->execution_end_time);
    {
      apc::ActionsResult_LatencyInformation_LatencyStep* latency_step =
          latency_info->add_latency_steps();
      latency_step->mutable_action()->set_action_index(i);
      latency_step->set_latency_start_ms(
          (action_result.start_time - actions_start_time).InMilliseconds());
      latency_step->set_latency_stop_ms(
          (*action_result.result->execution_end_time - actions_start_time)
              .InMilliseconds());
    }
    // Don't report a page stabilization time if the start and end
    // are the same. Not every tool needs stabilization.
    if (*action_result.result->execution_end_time != action_result.end_time) {
      apc::ActionsResult_LatencyInformation_LatencyStep* latency_step =
          latency_info->add_latency_steps();
      latency_step->mutable_page_stabilization()->set_action_index(i);
      latency_step->set_latency_start_ms(
          (*action_result.result->execution_end_time - actions_start_time)
              .InMilliseconds());
      latency_step->set_latency_stop_ms(
          (action_result.end_time - actions_start_time).InMilliseconds());
    }
  }

  std::vector<Browser*> browsers =
      chrome::FindAllTabbedBrowsersWithProfile(profile);

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

  absl::flat_hash_map<tabs::TabInterface*, apc::TabObservation*> tabs_to_fetch;

  ActorTask::TabHandleSet last_acted_tabs = task.GetLastActedTabs();
  for (const tabs::TabHandle& handle : last_acted_tabs) {
    // Include a TabObservation entry for acted on tabs. If the tab no longer
    // exists or the fetch context failed, the observation will be empty.
    // TODO(crbug.com/434263095): We should probably avoid capturing
    // observations if an action fails with kUrlBlocked. That might be better
    // implemented by not putting the tab into the LastActedTabs set.
    TabInterface* tab = handle.Get();
    if (!tab) {
      apc::TabObservation* tab_observation = response->add_tabs();
      tab_observation->set_id(handle.raw_value());
      tab_observation->set_result(
          apc::TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY);
      actor_service->GetJournal().Log(GURL(), task.id(),
                                      "TabObservationFailed",
                                      JournalDetailsBuilder()
                                          .Add("tabId", handle.raw_value())
                                          .AddError("TabWentAway")
                                          .Build());
    } else if (!tab->GetContents()
                    ->GetPrimaryMainFrame()
                    ->IsRenderFrameLive()) {
      // TODO(crbug.com/392167142): We should also handle the crashed subframes.
      // However we don't want unrelated subframe crash to terminate the task.
      apc::TabObservation* tab_observation = response->add_tabs();
      tab_observation->set_id(handle.raw_value());
      tab_observation->set_result(
          apc::TabObservation::TAB_OBSERVATION_PAGE_CRASHED);
      actor_service->GetJournal().Log(GURL(), task.id(),
                                      "TabObservationFailed",
                                      JournalDetailsBuilder()
                                          .Add("tabId", handle.raw_value())
                                          .AddError("Page crashed")
                                          .Build());
    } else {
      apc::TabObservation* tab_observation = response->add_tabs();
      tabs_to_fetch.emplace(tab, tab_observation);
      tab_observation->set_id(tab->GetHandle().raw_value());
      if (skip_async_observation_information) {
        tab_observation->set_result(apc::TabObservation::TAB_OBSERVATION_OK);
      }
    }
  }

  actor_service->GetJournal().Log(
      GURL(), TaskId(), "Observing Tabs",
      JournalDetailsBuilder()
          .Add("tab_observations", last_acted_tabs.size())
          .Add("tabs_to_fetch", tabs_to_fetch.size())
          .Build());

  base::UmaHistogramCounts1000("Actor.PageContext.TabCount",
                               tabs_to_fetch.size());

  if (skip_async_observation_information) {
    std::move(callback).Run(actions_start_time, result_code,
                            index_of_failed_action, std::move(action_results),
                            task.id(), skip_async_observation_information,
                            std::move(response), std::move(journal_entry));
    return;
  }
  base::RepeatingClosure barrier = base::BarrierClosure(
      tabs_to_fetch.size(),
      base::BindOnce(std::move(callback), actions_start_time, result_code,
                     index_of_failed_action, action_results,
                     task.id(), skip_async_observation_information,
                     std::move(response), std::move(journal_entry)));
  for (auto& [tab, tab_observation] : tabs_to_fetch) {
    // tab_observation can be Unretained because the underlying APC is owned
    // by the barrier which is ref-counted.
    actor_service->RequestTabObservation(
        *tab, task.id(),
        base::BindOnce(FetchCallback, tab->GetHandle(), profile->GetWeakPtr(),
                       task.id(), barrier, base::Unretained(tab_observation),
                       action_results, actions_start_time,
                       base::TimeTicks::Now(), base::Unretained(latency_info)));
  }
}

void SetTabObservationResultOverrideForTesting(
    base::RepeatingCallback<
        optimization_guide::proto::TabObservation::TabObservationResult()>
        callback) {
  GetTabObservationResultOverrideForTesting() = callback;
}

apc::ActionsResult BuildErrorActionsResult(
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) {
  TRACE_EVENT0("actor", "BuildErrorActionsResult");
  apc::ActionsResult response;
  CHECK(!IsOk(result_code));

  response.set_action_result(static_cast<int32_t>(result_code));
  if (index_of_failed_action) {
    response.set_index_of_failed_action(*index_of_failed_action);
  }

  return response;
}

std::string ToBase64(const optimization_guide::proto::Actions& actions) {
  TRACE_EVENT0("actor", "ActionsToBase64");
  size_t size = actions.ByteSizeLong();
  std::vector<uint8_t> buffer(size);
  actions.SerializeToArray(buffer.data(), size);
  return base::Base64Encode(buffer);
}

std::unique_ptr<page_content_annotations::FetchPageProgressListener>
CreateActorJournalFetchPageProgressListener(
    base::SafeRef<AggregatedJournal> journal,
    const GURL& url,
    TaskId task_id) {
  return std::make_unique<ActorJournalFetchPageProgressListener>(journal, url,
                                                                 task_id);
}

}  // namespace actor
