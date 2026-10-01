// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal.h"
#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/journal_details_builder.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "url/gurl.h"
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

}  // namespace

ToolController::ToolController(SessionControllerImpl& session_controller)
    : session_controller_(session_controller) {
  // Start the task up front so that the whole session is journaled under it.
  // TtcKeyedServiceFactory doesn't create TTC without the actor service.
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);
  EnsureTaskCreated(actor_service);
}

ToolController::~ToolController() {
  if (!task_id_.is_null()) {
    auto* actor_service = actor::ActorKeyedService::Get(GetProfile());
    CHECK(actor_service);

    // Cancel a task that never acted so that it isn't surfaced to the user.
    // TODO(b/544821996): Revisit once TTC has its own task client and delegate.
    actor::ActorTask::StoppedReason stop_reason =
        actor::ActorTask::StoppedReason::kTaskComplete;
    if (const actor::ActorTask* task = actor_service->GetTask(task_id_);
        task && task->GetState() == actor::ActorTask::State::kCreated) {
      stop_reason = actor::ActorTask::StoppedReason::kStoppedByUser;
    }
    actor_service->StopTask(task_id_, stop_reason);
  }
}

void ToolController::ProcessToolCall(const ToolRequest& tool_request,
                                     ToolResponseCallback callback) {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);

  // Done before journaling the call so that, if the task has to be replaced,
  // the call is journaled under the task it runs in.
  EnsureTaskCreated(actor_service);

  callback = base::BindOnce(
      &OnToolCallFinished,
      journal_->BeginAsyncEvent(
          "TtcToolCall",
          actor::JournalDetailsBuilder()
              .Add("name", tool_request.name)
              .Add("arguments",
                   base::WriteJson(tool_request.arguments).value_or(""))
              .Build()),
      std::move(callback));

#if !BUILDFLAG(IS_ANDROID)
  if (tool_request.name == "open_url") {
    OpenUrl(tool_request.arguments, std::move(callback));
    return;
  }
  if (tool_request.name == "perform_search") {
    PerformSearch(tool_request.arguments, std::move(callback));
    return;
  }
  if (tool_request.name == "close_current_tab") {
    CloseCurrentTab(std::move(callback));
    return;
  }
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
  if (tool_request.name == "switch_tab") {
    SwitchTab(tool_request.arguments, std::move(callback));
    return;
  }
  if (tool_request.name == "open_known_page") {
    OpenKnownPage(tool_request.arguments, std::move(callback));
    return;
  }
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
#endif

  std::move(callback).Run(ToolResponse::Error(
      actor::mojom::ActionResultCode::kToolUnknown, "Unsupported tool"));
}

std::vector<ToolDefinition> ToolController::GetToolDefinitions() {
  std::vector<ToolDefinition> tools;

#if !BUILDFLAG(IS_ANDROID)
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

  ToolDefinition perform_search;
  perform_search.name = "perform_search";
  perform_search.description = "Search using the default search engine.";
  perform_search.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue()
                   .Set("query",
                        base::DictValue()
                            .Set("type", "string")
                            .Set("description", "The terms to search for."))
                   .Set("new_tab",
                        base::DictValue()
                            .Set("type", "boolean")
                            .Set("description",
                                 "If true, performs the search in a new tab; "
                                 "otherwise, navigates the current tab.")))
          .Set("required", base::ListValue().Append("query").Append("new_tab"));
  perform_search.behavior = ToolDefinition::Behavior::kBlocking;
  perform_search.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(perform_search));

  ToolDefinition close_current_tab;
  close_current_tab.name = "close_current_tab";
  close_current_tab.description = "Close the current browser tab.";
  // The tool takes no arguments, so its schema is left empty.
  close_current_tab.behavior = ToolDefinition::Behavior::kBlocking;
  close_current_tab.verbalization =
      ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(close_current_tab));

  ToolDefinition go_back;
  go_back.name = "go_back";
  go_back.description = "Go back to the previous page in history.";
  // The tool takes no arguments, so its schema is left empty.
  go_back.behavior = ToolDefinition::Behavior::kBlocking;
  go_back.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(go_back));

  ToolDefinition go_forward;
  go_forward.name = "go_forward";
  go_forward.description = "Go forward to the next page in history.";
  // The tool takes no arguments, so its schema is left empty.
  go_forward.behavior = ToolDefinition::Behavior::kBlocking;
  go_forward.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(go_forward));

  ToolDefinition reload_page;
  reload_page.name = "reload_page";
  reload_page.description = "Reload the current page.";
  // The tool takes no arguments, so its schema is left empty.
  reload_page.behavior = ToolDefinition::Behavior::kBlocking;
  reload_page.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(reload_page));

  ToolDefinition switch_tab;
  switch_tab.name = "switch_tab";
  switch_tab.description =
      "Switches to an open tab in the current browser window matching the "
      "query.";
  switch_tab.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "query",
                   base::DictValue()
                       .Set("type", "string")
                       .Set("description",
                            "The search query to match against open tab "
                            "titles and URLs.")))
          .Set("required", base::ListValue().Append("query"));
  switch_tab.behavior = ToolDefinition::Behavior::kBlocking;
  switch_tab.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(switch_tab));

  ToolDefinition open_known_page;
  open_known_page.name = "open_known_page";
  open_known_page.description =
      "Opens a known page matching the query by searching open tabs, "
      "browsing history, and bookmarks.";
  open_known_page.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "query",
                   base::DictValue()
                       .Set("type", "string")
                       .Set("description",
                            "The search query to match against open tabs, "
                            "browsing history, and bookmarks.")))
          .Set("required", base::ListValue().Append("query"));
  open_known_page.behavior = ToolDefinition::Behavior::kBlocking;
  open_known_page.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(open_known_page));

  ToolDefinition find_and_highlight;
  find_and_highlight.name = "find_and_highlight";
  find_and_highlight.description =
      "Highlight and scroll to specific text on the page.";
  find_and_highlight.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "query", base::DictValue()
                                .Set("type", "string")
                                .Set("description",
                                     "A short phrase copied exactly as it "
                                     "appears in the page content.")))
          .Set("required", base::ListValue().Append("query"));
  find_and_highlight.behavior = ToolDefinition::Behavior::kBlocking;
  find_and_highlight.verbalization =
      ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(find_and_highlight));

  ToolDefinition play_video;
  play_video.name = "play_video";
  play_video.description = "Resume video playback.";
  // The tool takes no arguments, so its schema is left empty.
  play_video.behavior = ToolDefinition::Behavior::kBlocking;
  play_video.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(play_video));

  ToolDefinition pause_video;
  pause_video.name = "pause_video";
  pause_video.description = "Pause video playback.";
  // The tool takes no arguments, so its schema is left empty.
  pause_video.behavior = ToolDefinition::Behavior::kBlocking;
  pause_video.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(pause_video));

  ToolDefinition seek_to_timestamp;
  seek_to_timestamp.name = "seek_to_timestamp";
  seek_to_timestamp.description = "Jump the video to a specific timecode.";
  seek_to_timestamp.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "timecode",
                   base::DictValue()
                       .Set("type", "string")
                       .Set("description",
                            "The timecode to seek to, from the video "
                            "transcript. Format: \"1:45\", \"0:30\", "
                            "\"1:02:15\".")))
          .Set("required", base::ListValue().Append("timecode"));
  seek_to_timestamp.behavior = ToolDefinition::Behavior::kBlocking;
  seek_to_timestamp.verbalization =
      ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(seek_to_timestamp));
#endif

  return tools;
}

Profile* ToolController::GetProfile() {
  return session_controller_->GetProfile();
}

void ToolController::EnsureTaskCreated(
    actor::ActorKeyedService* actor_service) {
  // TODO(b/552544497): Ideally the task could only be stopped by `this`, but
  // there are currently a few ways for tasks to be stopped outside of this
  // class.
  if (!task_id_.is_null() && actor_service->GetTask(task_id_)) {
    return;
  }

  // The session this object belongs to is owned by the TtcKeyedService, so it
  // is guaranteed to exist.
  TtcKeyedService* ttc_service = TtcKeyedService::Get(GetProfile());
  CHECK(ttc_service);

  // TODO(b/544821996): Provide an ActorTaskDelegate.
  task_id_ = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc, "ttc"),
      actor::GetNullEnterprisePolicyChecker(), /*options=*/nullptr,
      /*delegate=*/nullptr, &ttc_service->actor_ui_state_manager());

  // The session keeps a single journal so that its async events stay
  // continuous if the task is replaced.
  if (journal_) {
    journal_->SetTaskId(task_id_);
  } else {
    journal_ =
        std::make_unique<SessionJournal>(actor_service->GetJournal(), task_id_);
  }
}

#if !BUILDFLAG(IS_ANDROID)
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

  PerformActionOnActiveTab(
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

  PerformActionOnActiveTab(
      [&query](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PerformSearchToolRequest>(tab_handle,
                                                                 *query);
      },
      std::move(callback));
}

void ToolController::CloseCurrentTab(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::CloseTabToolRequest>(tab_handle);
      },
      std::move(callback));
}

void ToolController::GoBack(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryToolRequest>(
            tab_handle, actor::HistoryToolRequest::Direction::kBack);
      },
      std::move(callback));
}

void ToolController::GoForward(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryToolRequest>(
            tab_handle, actor::HistoryToolRequest::Direction::kForward);
      },
      std::move(callback));
}

void ToolController::ReloadPage(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryToolRequest>(
            tab_handle, actor::HistoryToolRequest::Direction::kReload);
      },
      std::move(callback));
}

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

void ToolController::FindAndHighlight(const base::DictValue& arguments,
                                      ToolResponseCallback callback) {
  const std::string* query = arguments.FindString("query");
  if (!query) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing query argument"));
    return;
  }

  PerformActionOnActiveTab(
      [&query](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::FindAndHighlightToolRequest>(tab_handle,
                                                                    *query);
      },
      std::move(callback));
}

void ToolController::PlayVideo(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::MediaControlToolRequest>(
            tab_handle, actor::PlayMedia());
      },
      std::move(callback));
}

void ToolController::PauseVideo(ToolResponseCallback callback) {
  PerformActionOnActiveTab(
      [](tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::MediaControlToolRequest>(
            tab_handle, actor::PauseMedia());
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

  std::optional<actor::SeekMedia> seek =
      actor::SeekMedia::FromTimecode(*timecode);
  if (!seek) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kArgumentsInvalid, "Invalid timecode"));
    return;
  }

  PerformActionOnActiveTab(
      [&seek](
          tabs::TabHandle tab_handle) -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::MediaControlToolRequest>(tab_handle,
                                                                *seek);
      },
      std::move(callback));
}

void ToolController::PerformActionOnActiveTab(
    base::FunctionRef<std::unique_ptr<actor::ToolRequest>(tabs::TabHandle)>
        create_action,
    ToolResponseCallback callback) {
  // TODO(b/561651267): Get BrowserWindowInterface* from SessionControllerImpl
  // (or a class that manages the active window for the session).
  BrowserWindowInterface* browser = nullptr;
  if (auto* collection =
          ProfileBrowserCollection::GetForProfile(GetProfile())) {
    browser = collection->GetLastActiveBrowser();
  }
  if (!browser) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kWindowWentAway,
                            "No active browser window"));
    return;
  }

  tabs::TabInterface* active_tab = browser->GetTabStripModel()->GetActiveTab();
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
      task_id_, std::move(actions), actor::ActorTaskMetadata(),
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

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace ttc
