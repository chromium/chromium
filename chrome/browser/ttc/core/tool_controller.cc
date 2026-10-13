// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_registry.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"
#endif

namespace ttc {

namespace {

void OnToolCallFinished(
    std::unique_ptr<SessionJournal::PendingAsyncEvent> journal_event,
    ToolResponseCallback callback,
    ToolResponse response) {
  actor::JournalDetailsBuilder details;
  if (response.Ok()) {
    details.Add("result", "ok");
  } else {
    const ToolError& error = response.error();
    if (error.code) {
      details.Add("code", *error.code);
    }
    details.AddError(error.message.value_or("Tool call failed"));
  }
  journal_event->EndEntry(std::move(details).Build());
  std::move(callback).Run(std::move(response));
}

// Parses the `node_id` argument sent by the server. The model sees `node_id`
// as an integer, but the server post-processes it into a dictionary of the
// form {"document_id": <serialized_token>, "dom_node_id": <int>}.
base::expected<actor::PageTarget, std::string> ParseNodeIdArgument(
    const base::DictValue& arguments) {
  const base::DictValue* node_id = arguments.FindDict("node_id");
  if (!node_id) {
    return base::unexpected("Missing node_id argument");
  }

  std::optional<int> dom_node_id = node_id->FindInt("dom_node_id");
  const std::string* document_id = node_id->FindString("document_id");
  if (!dom_node_id || !document_id) {
    return base::unexpected("node_id must contain dom_node_id and document_id");
  }

  return actor::PageTarget(actor::DomNode{.node_id = *dom_node_id,
                                          .document_identifier = *document_id});
}

}  // namespace

ToolController::ToolController(SessionControllerImpl& session_controller)
    : session_controller_(session_controller) {}

ToolController::~ToolController() = default;

void ToolController::ProcessToolCall(const ToolRequest& tool_request,
                                     ToolResponseCallback callback) {
  callback = base::BindOnce(
      &OnToolCallFinished,
      session_controller_->GetJournal().BeginAsyncEvent(
          "TtcToolCall",
          actor::JournalDetailsBuilder()
              .Add("name", tool_request.name)
              .Add("arguments",
                   base::WriteJson(tool_request.arguments).value_or(""))
              .Build()),
      std::move(callback));

  if (tool_request.name == "open_url") {
    OpenUrl(tool_request.arguments, std::move(callback));
    return;
  }
  if (tool_request.name == "perform_search") {
    PerformSearch(tool_request.arguments, std::move(callback));
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (tool_request.name == "close_current_tab") {
    CloseCurrentTab(std::move(callback));
    return;
  }
#endif
  if (tool_request.name == "go_back") {
    GoBack(std::move(callback));
    return;
  }
  if (tool_request.name == "go_forward") {
    GoForward(std::move(callback));
    return;
  }
  if (tool_request.name == "reload_page") {
    ReloadPage(std::move(callback));
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (tool_request.name == "switch_tab") {
    SwitchTab(tool_request.arguments, std::move(callback));
    return;
  }
  if (tool_request.name == "open_known_page") {
    OpenKnownPage(tool_request.arguments, std::move(callback));
    return;
  }
#endif
  if (tool_request.name == "find_and_highlight") {
    FindAndHighlight(tool_request.arguments, std::move(callback));
    return;
  }

  if (tool_request.name == "play_video") {
    PlayVideo(std::move(callback));
    return;
  }

  if (tool_request.name == "pause_video") {
    PauseVideo(std::move(callback));
    return;
  }

  if (tool_request.name == "seek_to_timestamp") {
    SeekToTimestamp(tool_request.arguments, std::move(callback));
    return;
  }

  if (tool_request.name == "translate_page") {
    TranslatePage(tool_request.arguments, std::move(callback));
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (tool_request.name == "enter_fullscreen") {
    SetFullscreen(/*fullscreen=*/true, std::move(callback));
    return;
  }
  if (tool_request.name == "exit_fullscreen") {
    SetFullscreen(/*fullscreen=*/false, std::move(callback));
    return;
  }
#endif

  // TODO(b/573150754): Migrate click_element, set_text, and
  // select_option to kClick, kType, and kSelectOption once MES supports
  // rewriting dom_node_id for TOCTOU checks.
  if (tool_request.name == "click_element") {
    ClickElement(tool_request.arguments, std::move(callback));
    return;
  }

  if (tool_request.name == "set_text") {
    SetText(tool_request.arguments, std::move(callback));
    return;
  }

  if (tool_request.name == "select_option") {
    SelectOption(tool_request.arguments, std::move(callback));
    return;
  }

  std::move(callback).Run(ToolResponse::Error(
      actor::mojom::ActionResultCode::kToolUnknown, "Unsupported tool"));
}

std::vector<ToolDefinition> ToolController::GetToolDefinitions() {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);

  // TODO(b/573158576): Add a dedicated open_url tool to ToolRegistry and
  // include its ToolId here.
  // TODO(b/573150754): Add kClick, kType, and kSelectOption once MES supports
  // rewriting dom_node_id for TOCTOU checks and ProcessToolCall() dispatches
  // their registry names.
  static const base::NoDestructor<std::set<actor::ToolId>> kSupportedToolIds({
      actor::ToolId::kPerformSearch,
      actor::ToolId::kGoBack,
      actor::ToolId::kGoForward,
      actor::ToolId::kReloadPage,
      actor::ToolId::kFindAndHighlight,
      actor::ToolId::kPlayVideo,
      actor::ToolId::kPauseVideo,
      actor::ToolId::kSeekToTimestamp,
      actor::ToolId::kTranslatePage,
#if !BUILDFLAG(IS_ANDROID)
      actor::ToolId::kSwitchTab,
      actor::ToolId::kOpenKnownPage,
      actor::ToolId::kCloseTab,
      actor::ToolId::kEnterFullscreen,
      actor::ToolId::kExitFullscreen,
#endif
  });

  std::vector<const actor::ToolDefinition*> actor_tools =
      actor_service->tool_registry().GetToolsByIds(*kSupportedToolIds);

  // Legacy tools defined below: open_url, click_element, set_text, and
  // select_option.
  constexpr size_t kNumLegacyTools = 4;
  std::vector<ToolDefinition> tools;
  tools.reserve(actor_tools.size() + kNumLegacyTools);
  for (const actor::ToolDefinition* actor_tool : actor_tools) {
    ToolDefinition tool;
    tool.name = actor_tool->name;
    tool.description = actor_tool->description;
    tool.parameters_json_schema = actor_tool->parameters_json_schema.Clone();
    tool.behavior = ToolDefinition::Behavior::kBlocking;
    tool.verbalization = ToolDefinition::Verbalization::kSilentAction;
    tools.push_back(std::move(tool));
  }

  // TODO(b/573158576): Remove legacy open_url definition once a dedicated
  // open_url tool is registered in ToolRegistry and dispatched in
  // ProcessToolCall().
  ToolDefinition open_url;
  open_url.name = "open_url";
  open_url.description = "Opens a URL in the browser.";
  open_url.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue()
                   .Set("url", base::DictValue()
                                   .Set("type", "string")
                                   .Set("description",
                                        "The complete URL to open (e.g. "
                                        "\"https://example.com\")."))
                   .Set("new_tab",
                        base::DictValue()
                            .Set("type", "boolean")
                            .Set("description",
                                 "If true, opens the URL in a new tab; "
                                 "otherwise, navigates the current tab.")))
          .Set("required", base::ListValue().Append("url").Append("new_tab"));
  open_url.behavior = ToolDefinition::Behavior::kBlocking;
  open_url.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(open_url));

  // TODO(b/570042394): Remove legacy click_element, set_text, and
  // select_option definitions once MES supports rewriting dom_node_id for
  // TOCTOU checks and ProcessToolCall() dispatches kClick, kType, and
  // kSelectOption.
  ToolDefinition click_element;
  click_element.name = "click_element";
  click_element.description =
      "Click or toggle an interactive element (such as a button, radio "
      "button, or checkbox) on the active webpage.";
  click_element.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("node_id",
                                     base::DictValue()
                                         .Set("type", "integer")
                                         .Set("description",
                                              "The numeric node ID of the "
                                              "target element (e.g. 101).")))
          .Set("required", base::ListValue().Append("node_id"));
  click_element.behavior = ToolDefinition::Behavior::kBlocking;
  click_element.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(click_element));

  ToolDefinition set_text;
  set_text.name = "set_text";
  set_text.description =
      "Replace the contents of a text input field (such as a text box or text "
      "area) on the active webpage with the given text.";
  set_text.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue()
                   .Set("node_id",
                        base::DictValue()
                            .Set("type", "integer")
                            .Set("description",
                                 "The numeric node ID of the target input "
                                 "field (e.g. 101)."))
                   .Set("text",
                        base::DictValue()
                            .Set("type", "string")
                            .Set("description",
                                 "The text to enter into the input field.")))
          .Set("required", base::ListValue().Append("node_id").Append("text"));
  set_text.behavior = ToolDefinition::Behavior::kBlocking;
  set_text.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(set_text));

  ToolDefinition select_option;
  select_option.name = "select_option";
  select_option.description =
      "Select an option from a dropdown (<select>) element on the active "
      "webpage.";
  select_option.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set(
              "properties",
              base::DictValue()
                  .Set("node_id",
                       base::DictValue()
                           .Set("type", "integer")
                           .Set("description",
                                "The numeric node ID of the target dropdown "
                                "element (e.g. 101)."))
                  .Set("value", base::DictValue()
                                    .Set("type", "string")
                                    .Set("description",
                                         "The value of the <option> element to "
                                         "select.")))
          .Set("required", base::ListValue().Append("node_id").Append("value"));
  select_option.behavior = ToolDefinition::Behavior::kBlocking;
  select_option.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(select_option));

  return tools;
}

Profile* ToolController::GetProfile() {
  return session_controller_->GetProfile();
}

void ToolController::OpenUrl(const base::DictValue& arguments,
                             ToolResponseCallback callback) {
  const std::string* url = arguments.FindString("url");
  if (!url) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing url argument"));
    return;
  }

  bool new_tab = arguments.FindBool("new_tab").value_or(false);

  // TODO(b/561651267): Add support for opening in a new tab.
  if (new_tab) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kNotImplemented,
                            "New tab not supported yet"));
    return;
  }

  PerformActionOnTrackedContents(
      [&url](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::NavigateToolRequest>(tab_handle,
                                                            GURL(*url));
      },
      std::move(callback));
}

void ToolController::PerformSearch(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  const std::string* query = arguments.FindString("query");
  if (!query) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing query argument"));
    return;
  }

  bool new_tab = arguments.FindBool("new_tab").value_or(false);

  // TODO(b/561651267): Add support for searching in a new tab.
  if (new_tab) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kNotImplemented,
                            "New tab not supported yet"));
    return;
  }

  PerformActionOnTrackedContents(
      [&query](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PerformSearchToolRequest>(tab_handle,
                                                                 *query);
      },
      std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID)
void ToolController::CloseCurrentTab(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::CloseTabToolRequest>(tab_handle);
      },
      std::move(callback));
}
#endif

void ToolController::GoBack(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryBackToolRequest>(tab_handle);
      },
      std::move(callback));
}

void ToolController::GoForward(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryForwardToolRequest>(tab_handle);
      },
      std::move(callback));
}

void ToolController::ReloadPage(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::ReloadPageToolRequest>(tab_handle);
      },
      std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID)
void ToolController::SwitchTab(const base::DictValue& arguments,
                               ToolResponseCallback callback) {
  const std::string* query = arguments.FindString("query");
  if (!query || query->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty query argument"));
    return;
  }

  PerformAction(std::make_unique<actor::SwitchTabToolRequest>(*query),
                std::move(callback));
}

void ToolController::OpenKnownPage(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  const std::string* query = arguments.FindString("query");
  if (!query || query->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty query argument"));
    return;
  }

  PerformAction(std::make_unique<actor::OpenKnownPageToolRequest>(*query),
                std::move(callback));
}
#endif

void ToolController::FindAndHighlight(const base::DictValue& arguments,
                                      ToolResponseCallback callback) {
  const std::string* query = arguments.FindString("query");
  if (!query) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing query argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&query](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::FindAndHighlightToolRequest>(tab_handle,
                                                                    *query);
      },
      std::move(callback));
}

void ToolController::PlayVideo(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PlayMediaToolRequest>(tab_handle);
      },
      std::move(callback));
}

void ToolController::PauseVideo(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PauseMediaToolRequest>(tab_handle);
      },
      std::move(callback));
}

void ToolController::SeekToTimestamp(const base::DictValue& arguments,
                                     ToolResponseCallback callback) {
  const std::string* timecode = arguments.FindString("timecode");
  if (!timecode) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing timecode argument"));
    return;
  }

  std::optional<base::TimeDelta> seek_time =
      actor::SeekMediaToolRequest::FromTimecode(*timecode);
  if (!seek_time) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kArgumentsInvalid, "Invalid timecode"));
    return;
  }

  PerformActionOnTrackedContents(
      [&seek_time](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::SeekMediaToolRequest>(tab_handle,
                                                             *seek_time);
      },
      std::move(callback));
}

void ToolController::TranslatePage(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  // If `target_language` is empty, the page is translated to the user's
  // preferred language.
  const std::string* target_language = arguments.FindString("target_language");
  if (!target_language) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing target_language argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&target_language](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::TranslatePageToolRequest>(
            tab_handle, *target_language);
      },
      std::move(callback));
}

void ToolController::ClickElement(const base::DictValue& arguments,
                                  ToolResponseCallback callback) {
  base::expected<actor::PageTarget, std::string> page_target =
      ParseNodeIdArgument(arguments);
  if (!page_target.has_value()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            page_target.error()));
    return;
  }

  PerformActionOnTrackedContents(
      [&page_target](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::ClickToolRequest>(
            tab_handle, *page_target, actor::mojom::ClickType::kLeft,
            actor::mojom::ClickCount::kSingle);
      },
      std::move(callback));
}

void ToolController::SetText(const base::DictValue& arguments,
                             ToolResponseCallback callback) {
  base::expected<actor::PageTarget, std::string> page_target =
      ParseNodeIdArgument(arguments);
  if (!page_target.has_value()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            page_target.error()));
    return;
  }

  const std::string* text = arguments.FindString("text");
  if (!text) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing text argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&page_target, &text](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::TypeToolRequest>(
            tab_handle, *page_target, *text,
            /*follow_by_enter=*/false, actor::TypeToolRequest::Mode::kReplace);
      },
      std::move(callback));
}

void ToolController::SelectOption(const base::DictValue& arguments,
                                  ToolResponseCallback callback) {
  base::expected<actor::PageTarget, std::string> page_target =
      ParseNodeIdArgument(arguments);
  if (!page_target.has_value()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            page_target.error()));
    return;
  }

  const std::string* value = arguments.FindString("value");
  if (!value) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing value argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&page_target, &value](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::SelectToolRequest>(tab_handle,
                                                          *page_target, *value);
      },
      std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID)
void ToolController::SetFullscreen(bool fullscreen,
                                   ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [fullscreen](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        // The tracked tab is always in a window, so this can't be null.
        const BrowserWindowInterface& browser =
            CHECK_DEREF(tab_handle.Get()->GetBrowserWindowInterface());
        const int32_t window_id = browser.GetSessionID().id();
        if (fullscreen) {
          return std::make_unique<actor::EnterFullscreenToolRequest>(window_id);
        }
        return std::make_unique<actor::ExitFullscreenToolRequest>(window_id);
      },
      std::move(callback));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ToolController::PerformActionOnTrackedContents(
    base::FunctionRef<std::unique_ptr<actor::ToolRequest>(tabs::TabHandle)>
        create_action,
    ToolResponseCallback callback) {
  // Resolved via the session's VoiceFocusedContentsTracker rather than a
  // TabStripModel, which Android doesn't have.
  // TODO(crbug.com/570587982): The Android tracker and the tools routed
  // through it here are only covered by tests on desktop.
  content::WebContents* contents =
      session_controller_->GetVoiceFocusedWebContents();
  tabs::TabInterface* active_tab =
      contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
  if (!active_tab) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }

  PerformAction(create_action(active_tab->GetHandle()), std::move(callback));
}

void ToolController::PerformAction(std::unique_ptr<actor::ToolRequest> action,
                                   ToolResponseCallback callback) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);

  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(std::move(action));

  actor_service->PerformActions(
      session_controller_->GetActorTaskId(), std::move(actions),
      actor::ActorTaskMetadata(),
      base::BindOnce(&ToolController::OnActionsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ToolController::OnActionsFinished(
    ToolResponseCallback callback,
    std::vector<actor::ActionResultWithLatencyInfo> results,
    actor::TabObservationStrategy strategy) {
  CHECK(!results.empty());

  const actor::mojom::ActionResult& result = *results[0].result;
  if (!actor::IsOk(result)) {
    std::move(callback).Run(ToolResponse::Error(result.code, result.message));
    return;
  }
  std::move(callback).Run(ToolResponse::Success());
}

}  // namespace ttc
