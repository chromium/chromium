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

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/open_url_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_registry.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/shared_types.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace ttc {

namespace {

void OnToolCallFinished(
    std::unique_ptr<SessionJournalImpl::PendingAsyncEvent> journal_event,
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

std::optional<actor::ScrollPageToolRequest::Granularity> ParseScrollGranularity(
    std::string_view granularity) {
  using Granularity = actor::ScrollPageToolRequest::Granularity;
  if (granularity == "page") {
    return Granularity::kPage;
  }
  if (granularity == "document") {
    return Granularity::kDocument;
  }
  return std::nullopt;
}

std::optional<actor::ScrollPageToolRequest::Direction> ParseScrollDirection(
    std::string_view direction) {
  using Direction = actor::ScrollPageToolRequest::Direction;
  if (direction == "up") {
    return Direction::kUp;
  }
  if (direction == "down") {
    return Direction::kDown;
  }
  if (direction == "left") {
    return Direction::kLeft;
  }
  if (direction == "right") {
    return Direction::kRight;
  }
  return std::nullopt;
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

  if (std::optional<actor::ToolId> tool_id =
          actor::ToolRegistry::NameToToolId(tool_request.name)) {
    switch (*tool_id) {
      case actor::ToolId::kPerformSearch:
        PerformSearch(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kGoBack:
        GoBack(std::move(callback));
        return;
      case actor::ToolId::kGoForward:
        GoForward(std::move(callback));
        return;
      case actor::ToolId::kReloadPage:
        ReloadPage(std::move(callback));
        return;
      case actor::ToolId::kFindAndHighlight:
        FindAndHighlight(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kPlayVideo:
        PlayVideo(std::move(callback));
        return;
      case actor::ToolId::kPauseVideo:
        PauseVideo(std::move(callback));
        return;
      case actor::ToolId::kSeekToTimestamp:
        SeekToTimestamp(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kTranslatePage:
        TranslatePage(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kPromptGeminiPanel:
        PromptGeminiPanel(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kCloseGeminiPanel:
        CloseGeminiPanel(std::move(callback));
        return;
      case actor::ToolId::kCloseTab:
        CloseCurrentTab(std::move(callback));
        return;
      case actor::ToolId::kSwitchTab:
        SwitchTab(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kOpenUrl:
        OpenUrl(tool_request.arguments, std::move(callback));
        return;
      case actor::ToolId::kEnterFullscreen:
        SetFullscreen(/*fullscreen=*/true, std::move(callback));
        return;
      case actor::ToolId::kExitFullscreen:
        SetFullscreen(/*fullscreen=*/false, std::move(callback));
        return;
      case actor::ToolId::kOpenKnownPage:
        OpenKnownPage(tool_request.arguments, std::move(callback));
        return;
      default:
        break;
    }
  }

  // TODO(b/573150754): Migrate click_element, set_text, select_option, and
  // scroll_page to kClick, kType, kSelectOption, and kScrollPage once MES
  // supports rewriting dom_node_id for TOCTOU checks.
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

  if (tool_request.name == "scroll_page") {
    ScrollPage(tool_request.arguments, std::move(callback));
    return;
  }

  if (tool_request.name == "get_screenshot") {
    GetScreenshot(std::move(callback));
    return;
  }

  std::move(callback).Run(ToolResponse::Error(
      actor::mojom::ActionResultCode::kToolUnknown, "Unsupported tool"));
}

std::vector<ToolDefinition> ToolController::GetToolDefinitions() {
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);

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
      actor::ToolId::kPromptGeminiPanel,
      actor::ToolId::kCloseGeminiPanel,
      actor::ToolId::kCloseTab,
      actor::ToolId::kSwitchTab,
      actor::ToolId::kOpenUrl,
      actor::ToolId::kEnterFullscreen,
      actor::ToolId::kExitFullscreen,
      actor::ToolId::kOpenKnownPage,
  });

  std::vector<const actor::ToolDefinition*> actor_tools =
      actor_service->tool_registry().GetToolsByIds(*kSupportedToolIds);

  // Legacy tools defined below: click_element, set_text, select_option,
  // scroll_page, and get_screenshot.
  constexpr size_t kNumLegacyTools = 5;
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

  // TODO(b/573150754): Remove legacy click_element, set_text, select_option,
  // and scroll_page definitions once MES supports rewriting dom_node_id for
  // TOCTOU checks and ProcessToolCall() dispatches kClick, kType,
  // kSelectOption, and kScrollPage.
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

  ToolDefinition scroll_page;
  scroll_page.name = "scroll_page";
  scroll_page.description =
      "Scrolls the page or a specific scrollable element in the given "
      "direction by one page, or all the way to its start or end.";
  scroll_page.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set(
              "properties",
              base::DictValue()
                  .Set("granularity",
                       base::DictValue()
                           .Set("type", "string")
                           .Set("description",
                                "The units to scroll by. \"page\" scrolls by "
                                "one page of the scroller. \"document\" "
                                "scrolls to the start or end of the "
                                "scroller.")
                           .Set("enum", base::ListValue().Append("page").Append(
                                            "document")))
                  .Set("direction",
                       base::DictValue()
                           .Set("type", "string")
                           .Set("description", "The direction to scroll.")
                           .Set("enum", base::ListValue()
                                            .Append("up")
                                            .Append("down")
                                            .Append("left")
                                            .Append("right")))
                  .Set("node_id",
                       base::DictValue()
                           .Set("type", "integer")
                           .Set("description",
                                "The DOM node ID of the scrollable element, "
                                "or 0 to scroll the main viewport.")))
          .Set("required", base::ListValue()
                               .Append("granularity")
                               .Append("direction")
                               .Append("node_id"));
  scroll_page.behavior = ToolDefinition::Behavior::kBlocking;
  scroll_page.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(scroll_page));

  ToolDefinition get_screenshot;
  get_screenshot.name = "get_screenshot";
  get_screenshot.description =
      "Capture a screenshot of the active webpage's current viewport.";
  // The tool takes no arguments, so its schema is left empty.
  get_screenshot.behavior = ToolDefinition::Behavior::kBlocking;
  get_screenshot.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(get_screenshot));

  return tools;
}

Profile* ToolController::GetProfile() {
  return session_controller_->GetProfile();
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
      [&page_target](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::ClickToolRequest>(
            actor_surface_handle, *page_target, actor::mojom::ClickType::kLeft,
            actor::mojom::ClickCount::kSingle);
      },
      std::move(callback));
}

void ToolController::CloseCurrentTab(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::CloseTabToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::CloseGeminiPanel(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::CloseGeminiPanelToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::FindAndHighlight(const base::DictValue& arguments,
                                      ToolResponseCallback callback) {
  const std::string* query =
      arguments.FindString(actor::FindAndHighlightToolRequest::kQueryParam);
  if (!query) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing query argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&query](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::FindAndHighlightToolRequest>(
            actor_surface_handle, *query);
      },
      std::move(callback));
}

// TODO(b/573253710): Migrate this to be an actor tool once ActorKeyedService
// returns `ToolResponse`s.
void ToolController::GetScreenshot(ToolResponseCallback callback) {
  if (is_capturing_screenshot_) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureThrottled,
        "A screenshot capture is already in progress"));
    return;
  }

  content::WebContents* contents =
      session_controller_->GetVoiceFocusedWebContents();
  if (!contents) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }

  is_capturing_screenshot_ = true;
  content::Page& page = contents->GetPrimaryPage();
  const GURL& url = page.GetMainDocument().GetLastCommittedURL();

  IsUrlSupportedForPageContext(
      url, base::BindOnce(&ToolController::OnGetScreenshotUrlChecked,
                          weak_factory_.GetWeakPtr(), page.GetWeakPtr(),
                          std::move(callback)));
}

void ToolController::OnGetScreenshotUrlChecked(
    base::WeakPtr<content::Page> page,
    ToolResponseCallback callback,
    bool is_url_supported) {
  if (!is_url_supported) {
    is_capturing_screenshot_ = false;
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kUrlBlocked,
                            "URL is not supported for screenshot capture"));
    return;
  }

  content::WebContents* web_contents =
      session_controller_->GetVoiceFocusedWebContents();
  if (!web_contents) {
    is_capturing_screenshot_ = false;
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }

  if (!page || &*page != &web_contents->GetPrimaryPage()) {
    is_capturing_screenshot_ = false;
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureFailed,
        "Focused page changed before screenshot capture"));
    return;
  }

  if (web_contents->IsCrashed()) {
    is_capturing_screenshot_ = false;
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kRendererCrashed, "Renderer crashed"));
    return;
  }

  page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions
      collection_options;
  collection_options.screenshot_image_format =
      page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::kJpeg;
  collection_options.screenshot_compression_quality = page_content_annotations::
      ScreenshotOptions::ScreenshotCompressionQuality::kHigh;

  page_content_annotations::FetchPageContextOptions options;
  options.screenshot_options =
      page_content_annotations::ScreenshotOptions::ViewportOnly(
          /*paint_preview_options=*/std::nullopt, collection_options);

  screenshot_fetcher_ =
      std::make_unique<page_content_annotations::PageContextFetcher>(
          /*get_screenshot_service_callback=*/base::NullCallback(),
          /*progress_listener=*/nullptr,
          /*fetch_pdf_content_callback=*/base::NullCallback());
  screenshot_fetcher_->FetchStart(
      *web_contents, options,
      base::BindOnce(&ToolController::OnGetScreenshotFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ToolController::OnGetScreenshotFetched(
    ToolResponseCallback callback,
    base::expected<
        std::unique_ptr<page_content_annotations::FetchPageContextResult>,
        page_content_annotations::FetchPageContextErrorDetails> result) {
  is_capturing_screenshot_ = false;
  screenshot_fetcher_.reset();

  if (!result.has_value()) {
    auto error = actor::mojom::ActionResultCode::kScreenshotCaptureFailed;
    switch (result.error().error_code) {
      case page_content_annotations::FetchPageContextError::
          kWebContentsWentAway:
        error = actor::mojom::ActionResultCode::kTabWentAway;
        break;
      case page_content_annotations::FetchPageContextError::kWebContentsChanged:
        error = actor::mojom::ActionResultCode::kScreenshotCapturePageChanged;
        break;
      case page_content_annotations::FetchPageContextError::
          kPageContextNotEligible:
        error =
            actor::mojom::ActionResultCode::kScreenshotCapturePageNotEligible;
        break;
      default:
        break;
    }

    std::move(callback).Run(ToolResponse::Error(error, result.error().message));
    return;
  }

  // `PageContextFetcher` only reports success with a non-null result.
  CHECK(result.value());
  if (!result.value()->screenshot_result.has_value()) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureFailed,
        result.value()->screenshot_result.error().message));
    return;
  }

  page_content_annotations::ScreenshotResult& screenshot_result =
      *result.value()->screenshot_result;
  if (screenshot_result.screenshot_data.empty()) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureFailed,
        "Captured screenshot data is empty"));
    return;
  }

  base::DictValue result_dict;
  result_dict.Set(
      "image_b64",
      base::Base64Encode(std::move(screenshot_result.screenshot_data)));
  result_dict.Set("mime_type", screenshot_result.mime_type);
  std::move(callback).Run(ToolResponse::Success(std::move(result_dict)));
}

void ToolController::GoBack(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryBackToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::GoForward(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::HistoryForwardToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::OpenKnownPage(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  const std::string* query =
      arguments.FindString(actor::OpenKnownPageToolRequest::kQueryParam);
  if (!query || query->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty query argument"));
    return;
  }

  PerformAction(std::make_unique<actor::OpenKnownPageToolRequest>(*query),
                std::move(callback));
}

void ToolController::OpenUrl(const base::DictValue& arguments,
                             ToolResponseCallback callback) {
  const std::string* url =
      arguments.FindString(actor::OpenUrlToolRequest::kUrlParam);
  if (!url || url->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty url argument"));
    return;
  }

  std::optional<bool> new_tab =
      arguments.FindBool(actor::OpenUrlToolRequest::kNewTabParam);
  if (!new_tab) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing new_tab argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&url, &new_tab](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::OpenUrlToolRequest>(
            actor_surface_handle, GURL(*url), *new_tab);
      },
      std::move(callback));
}

void ToolController::PauseVideo(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PauseMediaToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::PerformSearch(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  const std::string* query =
      arguments.FindString(actor::PerformSearchToolRequest::kQueryParam);
  if (!query) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing query argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&query](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PerformSearchToolRequest>(
            actor_surface_handle, *query);
      },
      std::move(callback));
}

void ToolController::PlayVideo(ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PlayMediaToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::PromptGeminiPanel(const base::DictValue& arguments,
                                       ToolResponseCallback callback) {
  const std::string* prompt =
      arguments.FindString(actor::PromptGeminiPanelToolRequest::kPromptParam);
  if (!prompt || prompt->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty prompt argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&prompt](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::PromptGeminiPanelToolRequest>(
            glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
            actor_surface_handle, *prompt);
      },
      std::move(callback));
}

void ToolController::ReloadPage(ToolResponseCallback callback) {
  // The actor rejects every action on a crashed tab, including reloading it,
  // and leaves recovering the tab to its client (see
  // ExecutionEngine::KickOffNextAction()), so reload a crashed tab directly.
  content::WebContents* contents =
      session_controller_->GetVoiceFocusedWebContents();
  if (contents && contents->IsCrashed() && !crashed_tab_reload_callback_) {
    crashed_tab_reload_callback_ = std::move(callback);
    Observe(contents);
    contents->GetController().Reload(content::ReloadType::NORMAL,
                                     /*check_for_repost=*/true);
    // Reload() doesn't start a navigation if it asks the user to confirm
    // resubmitting a form.
    if (crashed_tab_reload_callback_ &&
        !contents->GetController().GetPendingEntry()) {
      FinishCrashedTabReload();
    }
    return;
  }

  PerformActionOnTrackedContents(
      [](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::ReloadPageToolRequest>(
            actor_surface_handle);
      },
      std::move(callback));
}

void ToolController::FinishCrashedTabReload() {
  Observe(nullptr);
  std::move(crashed_tab_reload_callback_).Run(ToolResponse::Success());
}

void ToolController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    FinishCrashedTabReload();
  }
}

void ToolController::ScrollPage(const base::DictValue& arguments,
                                ToolResponseCallback callback) {
  const std::string* granularity_arg =
      arguments.FindString(actor::ScrollPageToolRequest::kGranularityParam);
  std::optional<actor::ScrollPageToolRequest::Granularity> granularity =
      granularity_arg ? ParseScrollGranularity(*granularity_arg) : std::nullopt;
  if (!granularity) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or invalid granularity argument"));
    return;
  }

  const std::string* direction_arg =
      arguments.FindString(actor::ScrollPageToolRequest::kDirectionParam);
  std::optional<actor::ScrollPageToolRequest::Direction> direction =
      direction_arg ? ParseScrollDirection(*direction_arg) : std::nullopt;
  if (!direction) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or invalid direction argument"));
    return;
  }

  base::expected<actor::PageTarget, std::string> page_target =
      ParseNodeIdArgument(arguments);
  if (!page_target.has_value()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            page_target.error()));
    return;
  }

  PerformActionOnTrackedContents(
      [&page_target, &granularity,
       &direction](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::ScrollPageToolRequest>(
            actor_surface_handle, *page_target, *granularity, *direction);
      },
      std::move(callback));
}

void ToolController::SeekToTimestamp(const base::DictValue& arguments,
                                     ToolResponseCallback callback) {
  const std::string* timecode =
      arguments.FindString(actor::SeekMediaToolRequest::kTimecodeParam);
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
      [&seek_time](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::SeekMediaToolRequest>(
            actor_surface_handle, *seek_time);
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
      [&page_target, &value](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::SelectToolRequest>(actor_surface_handle,
                                                          *page_target, *value);
      },
      std::move(callback));
}

void ToolController::SetFullscreen(bool fullscreen,
                                   ToolResponseCallback callback) {
  PerformActionOnTrackedContents(
      [fullscreen](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        // The tracked tab is always in a window, so this can't be null.
        const BrowserWindowInterface& browser =
            CHECK_DEREF(actor_surface_handle.GetTabHandle()
                            .Get()
                            ->GetBrowserWindowInterface());
        const int32_t window_id = browser.GetSessionID().id();
        if (fullscreen) {
          return std::make_unique<actor::EnterFullscreenToolRequest>(window_id);
        }
        return std::make_unique<actor::ExitFullscreenToolRequest>(window_id);
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

  const std::string* text =
      arguments.FindString(actor::TypeToolRequest::kTextParam);
  if (!text) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing text argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&page_target, &text](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::TypeToolRequest>(
            actor_surface_handle, *page_target, *text,
            /*follow_by_enter=*/false, actor::TypeToolRequest::Mode::kReplace);
      },
      std::move(callback));
}

void ToolController::SwitchTab(const base::DictValue& arguments,
                               ToolResponseCallback callback) {
  const std::string* query =
      arguments.FindString(actor::SwitchTabToolRequest::kQueryParam);
  if (!query || query->empty()) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing or empty query argument"));
    return;
  }

  PerformAction(std::make_unique<actor::SwitchTabToolRequest>(*query),
                std::move(callback));
}

void ToolController::TranslatePage(const base::DictValue& arguments,
                                   ToolResponseCallback callback) {
  // If `target_language` is empty, the page is translated to the user's
  // preferred language.
  const std::string* target_language = arguments.FindString(
      actor::TranslatePageToolRequest::kTargetLanguageParam);
  if (!target_language) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing target_language argument"));
    return;
  }

  PerformActionOnTrackedContents(
      [&target_language](actor::ActorSurfaceHandle actor_surface_handle)
          -> std::unique_ptr<actor::ToolRequest> {
        return std::make_unique<actor::TranslatePageToolRequest>(
            actor_surface_handle, *target_language);
      },
      std::move(callback));
}

void ToolController::PerformActionOnTrackedContents(
    base::FunctionRef<std::unique_ptr<actor::ToolRequest>(
        actor::ActorSurfaceHandle)> create_action,
    ToolResponseCallback callback) {
  // Resolved via the session's VoiceFocusedContentsTracker rather than a
  // TabStripModel, which Android doesn't have.
  content::WebContents* contents =
      session_controller_->GetVoiceFocusedWebContents();
  tabs::TabInterface* active_tab =
      contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
  actor::ActorSurfaceHandle actor_surface_handle;
  if (active_tab) {
    actor_surface_handle =
        actor::ActorSurfaceHandle::From(active_tab->GetHandle());
  }
  if (actor_surface_handle.is_null()) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }

  PerformAction(create_action(actor_surface_handle), std::move(callback));
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
