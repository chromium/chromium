// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <set>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/gfx/skia_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace {

std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions() {
  // TODO(crbug.com/462208418): Use contextual tasks fieldtrial when available.
  auto image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

std::unique_ptr<FileData> ReadFileAndProcess(const base::FilePath& local_path) {
  auto file_data = std::make_unique<FileData>();

  if (!base::ReadFileToString(local_path, &file_data->bytes)) {
    LOG(ERROR) << "Failed to read file from path: "
               << local_path.AsUTF8Unsafe();
  }
  net::GetMimeTypeFromExtension(local_path.Extension().substr(1),
                                &file_data->mime_type);
  file_data->name = local_path.BaseName().AsUTF8Unsafe();
  return file_data;
}

class ContextualTasksOmniboxClient : public ContextualOmniboxClient {
 public:
  ContextualTasksOmniboxClient(
      Profile* profile,
      content::WebContents* web_contents,
      ContextualTasksComposeboxHandler* composebox_handler)
      : ContextualOmniboxClient(profile, web_contents),
        composebox_handler_(composebox_handler) {}
  ~ContextualTasksOmniboxClient() override = default;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::CO_BROWSING_COMPOSEBOX;
  }

  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  raw_ptr<ContextualTasksComposeboxHandler> composebox_handler_;
  raw_ptr<ContextualSearchboxHandler> contextual_searchbox_handler_;
};

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualTasksOmniboxClient::GetLensOverlaySuggestInputs() const {
  if (!contextual_tasks::GetIsContextualTasksSuggestionsEnabled()) {
    return lens::proto::LensOverlaySuggestInputs();
  }

  return SearchboxOmniboxClient::GetLensOverlaySuggestInputs();
}

void ContextualTasksOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  std::string query_text;
  net::GetValueForKeyInQuery(destination_url, "q", &query_text);
  composebox_handler_->CreateAndSendQueryMessage(query_text);
}

// The amount of change in bytes that is considered a significant change and
// should trigger a page content update request. This provides tolerance in
// case there is slight variation in the retrieved bytes in between calls.
constexpr float kByteChangeTolerancePercent = 0.01;

}  // namespace

ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler(
    ContextualTasksUI* ui_controller,
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    GetSessionHandleCallback get_session_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents,
                                                             this)),
          std::move(get_session_callback)),
      web_ui_controller_(ui_controller),
      contextual_tasks_service_(
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              profile)) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualOmniboxClient*>(omnibox_controller()->client())
      ->SetSuggestInputsCallback(base::BindRepeating(
          &ContextualTasksComposeboxHandler::GetSuggestInputs,
          base::Unretained(this)));
}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  ContextualSearchboxHandler::OnFileUploadStatusChanged(
      file_token, mime_type, file_upload_status, error_type);

  // Associate tab with task.
  if (file_upload_status ==
      contextual_search::FileUploadStatus::kUploadSuccessful) {
    auto* contextual_session_handle = GetContextualSessionHandle();
    if (!contextual_session_handle) {
      return;
    }

    const contextual_search::FileInfo* file_info =
        contextual_session_handle->GetController()->GetFileInfo(file_token);
    if (!file_info || !file_info->tab_session_id.has_value()) {
      return;
    }

    auto task_id = web_ui_controller_->GetTaskId();
    if (task_id.has_value() && contextual_tasks_service_) {
      contextual_tasks_service_->AssociateTabWithTask(
          task_id.value(), file_info->tab_session_id.value());
    }
  }
}

void ContextualTasksComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  CreateAndSendQueryMessage(query_text);
  // TODO(crbug.com/469535685): This should reflect the response from the
  // webview when PostMessageToWebview provides one.
}

void ContextualTasksComposeboxHandler::CreateAndSendQueryMessage(
    const std::string& query) {
  // Every time a query is submitted, close the Lens overlay if it's open.
  CloseLensOverlay(
      lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
  std::optional<base::Uuid> task_id = web_ui_controller_->GetTaskId();
  auto* contextual_tasks_service = GetContextualTasksService();
  if (!task_id.has_value() || !contextual_tasks_service) {
    ContinueCreateAndSendQueryMessage(query, task_id);
    return;
  }

  // Get the active tab handle now, as recontextualization is an async process
  // and fetching the active tab later may result in unexpected behavior.
  tabs::TabHandle active_tab_handle;
  auto* browser_window_interface = webui::GetBrowserWindowInterface(
      web_ui_controller_->GetWebUIWebContents());
  if (browser_window_interface) {
    auto* tab_strip_model = browser_window_interface->GetTabStripModel();
    if (tab_strip_model) {
      tabs::TabInterface* active_tab = tab_strip_model->GetActiveTab();
      if (active_tab) {
        active_tab_handle = active_tab->GetHandle();
      }
    }
  }

  // Fetch the context for the task, including pending context from the current
  // session handle.
  auto context_decoration_params =
      std::make_unique<contextual_tasks::ContextDecorationParams>();
  if (auto* session_handle = GetContextualSessionHandle()) {
    context_decoration_params->contextual_search_session_handle =
        session_handle->AsWeakPtr();
  }

  // TODO(crbug.com/468453630): The context needs to actually be populated
  // with tab data from the server-managed context list.
  contextual_tasks_service->GetContextForTask(
      *task_id,
      {contextual_tasks::ContextualTaskContextSource::kPendingContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&ContextualTasksComposeboxHandler::OnContextRetrieved,
                     weak_factory_.GetWeakPtr(), query, active_tab_handle,
                     /*task_id=*/task_id));
}

contextual_tasks::ContextualTasksService*
ContextualTasksComposeboxHandler::GetContextualTasksService() {
  return contextual_tasks_service_;
}

void ContextualTasksComposeboxHandler::OnContextRetrieved(
    std::string query,
    tabs::TabHandle active_tab_handle,
    std::optional<base::Uuid> original_task_id,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
  if (!context || web_ui_controller_->GetTaskId() != original_task_id) {
    ContinueCreateAndSendQueryMessage(query, original_task_id);
    return;
  }
  tabs::TabInterface* active_tab = active_tab_handle.Get();
  std::vector<tabs::TabInterface*> tabs_to_update =
      GetTabsToUpdate(*context, active_tab);

  if (tabs_to_update.empty()) {
    ContinueCreateAndSendQueryMessage(query, original_task_id);
    return;
  }

  // Use a barrier closure to wait for all tabs to be processed.
  // The callback will be invoked once for each tab.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      tabs_to_update.size(),
      base::BindOnce(
          &ContextualTasksComposeboxHandler::ContinueCreateAndSendQueryMessage,
          weak_factory_.GetWeakPtr(), query, original_task_id));

  for (tabs::TabInterface* tab : tabs_to_update) {
    if (!tab) {
      barrier_closure.Run();
      continue;
    }

    tabs::TabFeatures* tab_features = tab->GetTabFeatures();
    if (!tab_features) {
      barrier_closure.Run();
      continue;
    }

    lens::TabContextualizationController* controller =
        tab_features->tab_contextualization_controller();
    if (!controller) {
      barrier_closure.Run();
      continue;
    }

    // TODO(crbug.com/466470730): Replace usages of tab session id with tab
    // handle.
    int32_t tab_id = tab->GetHandle().raw_value();
    controller->GetPageContext(base::BindOnce(
        &ContextualTasksComposeboxHandler::OnTabContextualizationFetched,
        weak_factory_.GetWeakPtr(), query,
        // Create a copy of the context for each tab, so that the context can
        // be moved into the callback. This is fine because all member
        // variables are held by value, and all complex objects have explicit
        // copy constructors.
        std::make_unique<contextual_tasks::ContextualTaskContext>(*context),
        barrier_closure, original_task_id, tab_id));
  }
}

void ContextualTasksComposeboxHandler::OnTabContextualizationFetched(
    std::string query,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context,
    base::RepeatingClosure barrier_closure,
    std::optional<base::Uuid> original_task_id,
    int32_t tab_id,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data) {
    barrier_closure.Run();
    return;
  }

  if (web_ui_controller_->GetTaskId() != original_task_id) {
    barrier_closure.Run();
    return;
  }

  if (contextual_tasks::GetIsProtectedPageErrorEnabled() &&
      !page_content_data->is_page_context_eligible.value_or(false)) {
    web_ui_controller_->OnPageContextEligibilityChecked(false);
    barrier_closure.Run();
    return;
  }

  std::optional<int64_t> maybe_context_id = std::nullopt;
  if (page_content_data->tab_session_id.has_value()) {
    maybe_context_id = GetContextIdForTab(*context, *page_content_data);
  }

  if (!ShouldUploadTabContext(maybe_context_id, *page_content_data)) {
    barrier_closure.Run();
    return;
  }

  UploadTabContextWithData(
      tab_id, maybe_context_id, std::move(page_content_data),
      base::BindOnce(&ContextualTasksComposeboxHandler::OnTabContextReuploaded,
                     weak_factory_.GetWeakPtr(), query, barrier_closure,
                     original_task_id));
}

void ContextualTasksComposeboxHandler::OnTabContextReuploaded(
    std::string query,
    base::RepeatingClosure barrier_closure,
    std::optional<base::Uuid> original_task_id,
    bool success) {
  if (web_ui_controller_->GetTaskId() != original_task_id) {
    barrier_closure.Run();
    return;
  }
  barrier_closure.Run();
}

void ContextualTasksComposeboxHandler::OnTaskChanged() {
  delayed_tabs_.clear();
}

std::vector<tabs::TabInterface*>
ContextualTasksComposeboxHandler::GetTabsToUpdate(
    const contextual_tasks::ContextualTaskContext& context,
    tabs::TabInterface* active_tab) {
  std::set<tabs::TabInterface*> tabs_to_update;
  // TODO(crbug.com/469807132): Add suggested tabs to the list of tabs to
  // update.

  // Add delayed tabs to the list of tabs to update.
  for (const auto& [token, tab_id] : delayed_tabs_) {
    tabs::TabHandle handle = tabs::TabHandle(tab_id);
    if (tabs::TabInterface* tab = handle.Get()) {
      tabs_to_update.insert(tab);
    }
  }
  delayed_tabs_.clear();

  // TODO(crbug.com/468430623): Support updating multiple tabs.
  // Currently this only checks the active tab.
  if (!active_tab) {
    return std::vector<tabs::TabInterface*>(tabs_to_update.begin(),
                                            tabs_to_update.end());
  }

  // Check if the active tab is in the context.
  SessionID active_tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  if (!active_tab_session_id.is_valid()) {
    return std::vector<tabs::TabInterface*>(tabs_to_update.begin(),
                                            tabs_to_update.end());
  }

  if (GetMatchingAttachment(context,
                            active_tab->GetContents()->GetLastCommittedURL(),
                            active_tab_session_id)) {
    tabs_to_update.insert(active_tab);
  }

  return std::vector<tabs::TabInterface*>(tabs_to_update.begin(),
                                          tabs_to_update.end());
}

std::optional<int64_t> ContextualTasksComposeboxHandler::GetContextIdForTab(
    const contextual_tasks::ContextualTaskContext& context,
    const lens::ContextualInputData& page_content_data) {
  if (!page_content_data.tab_session_id.has_value()) {
    return std::nullopt;
  }
  SessionID tab_session_id = page_content_data.tab_session_id.value();

  auto* controller = GetContextualTasksService();
  if (!controller) {
    return std::nullopt;
  }

  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    return std::nullopt;
  }

  auto* search_context_controller = contextual_session_handle->GetController();
  if (!search_context_controller) {
    return std::nullopt;
  }

  if (!page_content_data.page_url.has_value()) {
    return std::nullopt;
  }

  if (GetMatchingAttachment(context, page_content_data.page_url.value(),
                            tab_session_id)) {
    const auto& file_info_list = search_context_controller->GetFileInfoList();
    for (const auto* file_info : file_info_list) {
      if (file_info->tab_session_id == tab_session_id) {
        return file_info->GetContextId();
      }
    }
  }
  return std::nullopt;
}

bool ContextualTasksComposeboxHandler::ShouldUploadTabContext(
    std::optional<int64_t> context_id,
    const lens::ContextualInputData& page_content_data) {
  // If the tab was not previously uploaded, or if the tab has expired, or if
  // the tab contents have changed significantly, the tab context should be
  // uploaded.
  if (!context_id.has_value()) {
    return true;
  }

  if (!page_content_data.tab_session_id.has_value()) {
    return true;
  }
  SessionID tab_session_id = page_content_data.tab_session_id.value();

  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    return true;
  }

  auto* search_context_controller = contextual_session_handle->GetController();
  if (!search_context_controller) {
    return true;
  }

  const auto& file_info_list = search_context_controller->GetFileInfoList();
  const contextual_search::FileInfo* matching_file_info = nullptr;
  for (const auto* file_info : file_info_list) {
    if (file_info->tab_session_id == tab_session_id) {
      matching_file_info = file_info;
      break;
    }
  }

  if (!matching_file_info) {
    return true;
  }

  if (matching_file_info->upload_status ==
      contextual_search::FileUploadStatus::kUploadExpired) {
    return true;
  }

  if (!matching_file_info->input_data) {
    return true;
  }

  const auto& old_data = *matching_file_info->input_data;
  const auto& new_data = page_content_data;

  // Check if primary content type changed.
  if (old_data.primary_content_type != new_data.primary_content_type) {
    return true;
  }

  // Check if page contents changed.
  if (new_data.primary_content_type.has_value()) {
    const std::vector<lens::ContextualInput>& old_inputs =
        old_data.context_input.has_value()
            ? *old_data.context_input
            : std::vector<lens::ContextualInput>();
    const std::vector<lens::ContextualInput>& new_inputs =
        new_data.context_input.has_value()
            ? *new_data.context_input
            : std::vector<lens::ContextualInput>();
    auto old_it = std::ranges::find_if(old_inputs, [&](const auto& input) {
      return input.content_type_ == new_data.primary_content_type.value();
    });
    auto new_it = std::ranges::find_if(new_inputs, [&](const auto& input) {
      return input.content_type_ == new_data.primary_content_type.value();
    });

    if (old_it != old_inputs.end() && new_it != new_inputs.end()) {
      const float old_size = old_it->bytes_.size();
      const float new_size = new_it->bytes_.size();
      if (old_size > 0) {
        const float percent_changed = abs((new_size - old_size) / old_size);
        if (percent_changed >= kByteChangeTolerancePercent) {
          return true;
        }
      } else if (new_size > 0) {
        return true;
      }
    } else if (old_it != old_inputs.end() || new_it != new_inputs.end()) {
      // One has content and the other doesn't.
      return true;
    }
  }

  // Check if viewport screenshot changed.
  // TODO(crbug.com/471960792): Add support for only recontextualizing the
  // screenshot when the viewport has changed but the page contents are the
  // same.

  // The screenshot may be in either the byte array or bitmap members of
  // ContextualInputData. Both should be checked for changes.
  bool old_has_screenshot = old_data.viewport_screenshot_bytes.has_value() &&
                            !old_data.viewport_screenshot_bytes->empty();
  bool new_has_screenshot = new_data.viewport_screenshot_bytes.has_value() &&
                            !new_data.viewport_screenshot_bytes->empty();

  if (old_has_screenshot != new_has_screenshot) {
    return true;
  }

  if (old_has_screenshot) {
    const auto& old_bytes = old_data.viewport_screenshot_bytes.value();
    const auto& new_bytes = new_data.viewport_screenshot_bytes.value();
    if (old_bytes.size() != new_bytes.size()) {
      return true;
    }
    // Exact byte comparison for screenshot.
    if (old_bytes != new_bytes) {
      return true;
    }
  }

  bool old_has_bitmap = old_data.viewport_screenshot.has_value();
  bool new_has_bitmap = new_data.viewport_screenshot.has_value();

  if (old_has_bitmap != new_has_bitmap) {
    return true;
  }

  if (old_has_bitmap) {
    const auto& old_bitmap = old_data.viewport_screenshot.value();
    const auto& new_bitmap = new_data.viewport_screenshot.value();
    if (!new_bitmap.drawsNothing() &&
        (old_bitmap.drawsNothing() ||
         !gfx::BitmapsAreEqual(old_bitmap, new_bitmap))) {
      return true;
    }
  }

  return false;
}

const contextual_tasks::UrlAttachment*
ContextualTasksComposeboxHandler::GetMatchingAttachment(
    const contextual_tasks::ContextualTaskContext& context,
    const GURL& url,
    SessionID session_id) {
  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      url_duplication_helper =
          contextual_tasks::CreateURLDeduplicationHelperForContextualTask();
  std::vector<const contextual_tasks::UrlAttachment*> matching_attachments =
      context.GetMatchingUrlAttachments(url, url_duplication_helper.get());

  for (const auto* attachment : matching_attachments) {
    if (attachment->GetTabSessionId() == session_id) {
      return attachment;
    }
  }
  return nullptr;
}

void ContextualTasksComposeboxHandler::ContinueCreateAndSendQueryMessage(
    std::string query,
    std::optional<base::Uuid> original_task_id) {
  if (web_ui_controller_->GetTaskId() != original_task_id) {
    return;
  }
  // Create a client to aim message and send it to the page.
  if (auto* session_handle = GetContextualSessionHandle()) {
    // If there is an auto-added tab, the user sending the query means the
    // system should upload it.
    UploadSnapshotTabContextIfPresent();

    // Create a client to aim message and send it to the page.
    auto create_client_to_aim_request_info =
        std::make_unique<contextual_search::ContextualSearchContextController::
                             CreateClientToAimRequestInfo>();
    create_client_to_aim_request_info->query_text = query;
    create_client_to_aim_request_info->query_text_source =
        lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT;
    create_client_to_aim_request_info->query_start_time = base::Time::Now();

    omnibox::ChromeAimToolsAndModels tool_mode = GetAimToolMode();
    create_client_to_aim_request_info->deep_search_selected =
        tool_mode == omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH;
    create_client_to_aim_request_info->create_images_selected =
        tool_mode == omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN ||
        tool_mode ==
            omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD;

    create_client_to_aim_request_info->file_tokens = GetUploadedContextTokens();
    lens::ClientToAimMessage client_to_page_message =
        session_handle->CreateClientToAimRequest(
            std::move(create_client_to_aim_request_info));
    web_ui_controller_->PostMessageToWebview(client_to_page_message);
  }
}

void ContextualTasksComposeboxHandler::HandleFileUpload(bool is_image) {
  content::WebContents* web_contents =
      web_ui_controller_->GetWebUIWebContents();
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();

  if (!file_dialog_) {
    file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  }

  ui::SelectFileDialog::FileTypeInfo file_types;
  if (is_image) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("image/*", &extensions);
    file_types.extensions.push_back(extensions);
  } else {
    file_types.extensions = {{FILE_PATH_LITERAL("pdf")}};
  }
  file_types.include_all_files = true;

  file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                           /*title=*/std::u16string(),
                           /*default_path=*/base::FilePath(), &file_types,
                           /*file_type_index=*/0,
                           /*default_extension=*/base::FilePath::StringType(),
                           parent_window);
}

void ContextualTasksComposeboxHandler::FileSelected(
    const ui::SelectedFileInfo& file,
    int index) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFileAndProcess, file.path()),
      base::BindOnce(&ContextualTasksComposeboxHandler::OnFileRead,
                     weak_factory_.GetWeakPtr()));
  file_dialog_.reset();
}

void ContextualTasksComposeboxHandler::OnFileRead(
    std::unique_ptr<FileData> file_data) {
  if (file_data->bytes.empty()) {
    return;
  }

  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = file_data->name;
  file_info->is_deletable = true;
  file_info->mime_type = file_data->mime_type;

  if (file_data->mime_type.find("image") != std::string::npos) {
    file_info->image_data_url = "data:" + file_data->mime_type + ";base64," +
                                base::Base64Encode(file_data->bytes);
  }

  base::span<const uint8_t> file_data_span =
      base::as_bytes(base::span(file_data->bytes));

  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  mojo_base::BigBuffer file_data_buffer =
      mojo_base::BigBuffer(file_data_vector);

  AddFileContext(std::move(file_info), std::move(file_data_buffer),
                 base::DoNothing());
}

void ContextualTasksComposeboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  if (auto* session_handle = GetContextualSessionHandle()) {
    std::string mime_type = file_info->mime_type;
    session_handle->AddFileContext(
        mime_type, std::move(file_bytes), CreateImageEncodingOptions(),
        base::BindOnce(&ContextualTasksComposeboxHandler::OnFileAddedToSession,
                       weak_factory_.GetWeakPtr(), std::move(file_info),
                       std::move(callback)));
  }
}

void ContextualTasksComposeboxHandler::OnFileAddedToSession(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    AddFileContextCallback callback,
    const base::UnguessableToken& token) {
  ContextualSearchboxHandler::page_->AddFileContext(token,
                                                    std::move(file_info));
  std::move(callback).Run(token);
}

void ContextualTasksComposeboxHandler::FileSelectionCanceled() {
  file_dialog_.reset();
}

void ContextualTasksComposeboxHandler::AddTabContext(
    int32_t tab_id,
    bool delay_upload,
    AddTabContextCallback callback) {
  // The delay_upload flag is used to indicate that the tab was auto-added
  // via the composebox. In the contextual-tasks case, added tabs should be
  // contextualized as late as possible so that the viewport and APC are
  // as recent as possible, put the tab in delayed_tabs_ instead of using the
  // superclass method that contextualizes immediately and caches the tab
  // context for uploading in UploadSnapshotTabContextIfPresent.
  if (delay_upload) {
    base::UnguessableToken token = base::UnguessableToken::Create();
    delayed_tabs_[token] = tab_id;
    std::move(callback).Run(token);
    return;
  }

  ContextualSearchboxHandler::AddTabContext(tab_id, delay_upload,
                                            std::move(callback));
}

void ContextualTasksComposeboxHandler::HandleLensButtonClick() {
  if (!contextual_tasks::GetEnableLensInContextualTasks()) {
    return;
  }
  if (auto* controller = GetLensSearchController()) {
    controller->SetThumbnailCreatedCallback(base::BindRepeating(
        &ContextualTasksComposeboxHandler::OnLensThumbnailCreated,
        base::Unretained(this)));
    controller->OpenLensOverlay(
        lens::LensOverlayInvocationSource::kContextualTasksComposebox);
  }
}

void ContextualTasksComposeboxHandler::OnLensThumbnailCreated(
    const std::string& thumbnail_data) {
  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "Visual Selection";
  file_info->mime_type = "image/png";
  file_info->image_data_url = thumbnail_data;
  file_info->is_deletable = true;

  // Clear any existing visual selection context.
  if (visual_selection_token_) {
    OnFileUploadStatusChanged(
        *visual_selection_token_, lens::MimeType::kUnknown,
        contextual_search::FileUploadStatus::kUploadExpired, std::nullopt);
  }

  // Also add the file context to the backend controller to ensure it is
  // uploaded.
  std::string_view data_view(thumbnail_data);
  size_t comma_pos = data_view.find(',');
  if (comma_pos != std::string_view::npos) {
    std::string_view base64_data = data_view.substr(comma_pos + 1);
    std::string decoded_data;
    if (base::Base64Decode(base64_data, &decoded_data)) {
      std::vector<uint8_t> data_vector(decoded_data.begin(),
                                       decoded_data.end());
      AddFileContext(
          std::move(file_info), mojo_base::BigBuffer(data_vector),
          base::BindOnce(
              &ContextualTasksComposeboxHandler::OnVisualSelectionAdded,
              weak_factory_.GetWeakPtr()));
    }
  }
}

void ContextualTasksComposeboxHandler::OnVisualSelectionAdded(
    const base::UnguessableToken& token) {
  if (visual_selection_token_.has_value()) {
    ComposeboxHandler::DeleteContext(visual_selection_token_.value(),
                                     /*from_automatic_chip=*/false);
  }
  visual_selection_token_ = token;
}

void ContextualTasksComposeboxHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {
  // Get file info before deletion.
  std::optional<SessionID> associated_tab_id;
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    const contextual_search::FileInfo* file_info =
        contextual_session_handle->GetController()->GetFileInfo(file_token);
    if (file_info && file_info->tab_session_id.has_value()) {
      associated_tab_id = file_info->tab_session_id.value();
    }
  }

  bool was_delayed = delayed_tabs_.erase(file_token);

  if (from_automatic_chip) {
    web_ui_controller_->DisableActiveTabContextSuggestion();
  }

  // Clear the visual selection token if it matches the deleted token.
  if (visual_selection_token_ && *visual_selection_token_ == file_token) {
    visual_selection_token_ = std::nullopt;
    // If the user explicitly deleted the context (not from automatic chip),
    // close the Lens Overlay.
    if (!from_automatic_chip) {
      if (auto* controller = GetLensSearchController()) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::kContextualTasksContextCleared);
      }
    }
  }

  if (!was_delayed) {
    ComposeboxHandler::DeleteContext(file_token, from_automatic_chip);
  } else {
    OnFileUploadStatusChanged(
        file_token, lens::MimeType::kUnknown,
        contextual_search::FileUploadStatus::kUploadExpired, std::nullopt);
  }

  // Hide the underline for the tab if it was associated with the deleted
  // context.
  if (associated_tab_id.has_value()) {
    auto* browser_window_interface = webui::GetBrowserWindowInterface(
        web_ui_controller_->GetWebUIWebContents());
    auto* active_task_context_provider =
        browser_window_interface
            ? browser_window_interface->GetFeatures()
                  .contextual_tasks_active_task_context_provider()
            : nullptr;
    if (active_task_context_provider) {
      active_task_context_provider->RefreshContext();
    }
  }
}

void ContextualTasksComposeboxHandler::CloseLensOverlay(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensSync(dismissal_source);
  }
}

LensSearchController*
ContextualTasksComposeboxHandler::GetLensSearchController() const {
  auto* browser = web_ui_controller_->GetBrowser();
  if (!browser) {
    return nullptr;
  }

  if (auto* controller = LensSearchController::FromTabWebContents(
          browser->GetTabStripModel()->GetActiveWebContents());
      controller) {
    return controller;
  }
  return nullptr;
}
