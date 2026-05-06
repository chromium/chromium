// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <set>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/aim_query.pb.h"
#include "ui/gfx/skia_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace {


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
    return std::nullopt;
  }

  return ContextualOmniboxClient::GetLensOverlaySuggestInputs();
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

lens::LensOverlayDismissalSource ToLensOverlayDismissalSource(
    composebox::mojom::LensOverlayDismissalSource dismissal_source) {
  switch (dismissal_source) {
    case composebox::mojom::LensOverlayDismissalSource::
        kContextualTasksImageUploadsDisabled:
      return lens::LensOverlayDismissalSource::
          kContextualTasksImageUploadsDisabled;
  }
  NOTREACHED() << "Unknown dismissal source: "
               << static_cast<int>(dismissal_source);
}

}  // namespace

ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler(
    contextual_tasks::ContextualTasksUIInterface* web_ui_interface,
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback,
    TakeInputStateModelCallback take_input_model_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          std::move(pending_searchbox_page),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents,
                                                             this)),
          std::move(get_session_callback),
          std::move(clear_session_callback)),
      take_input_model_callback_(std::move(take_input_model_callback)),
      web_ui_interface_(web_ui_interface),
      contextual_tasks_service_(
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              profile)),
      desktop_delegate_(std::make_unique<
                        contextual_tasks::DesktopQueryContextualizerDelegate>(
          base::BindRepeating(
              &ContextualTasksComposeboxHandler::GetContextualSessionHandle,
              base::Unretained(this)),
          base::BindRepeating(
              &ContextualSearchboxHandler::CreateImageEncodingOptions),
          contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
              profile),
          webui::GetBrowserWindowInterface(
              web_ui_interface->GetWebUIWebContents()))),
      recontextualizer_(std::make_unique<contextual_tasks::QueryContextualizer>(
          contextual_tasks_service_,
          desktop_delegate_.get())) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualTasksOmniboxClient*>(omnibox_controller()->client())
      ->SetSuggestInputsCallback(base::BindRepeating(
          &ContextualTasksComposeboxHandler::GetSuggestInputs,
          base::Unretained(this)));

  if (contextual_tasks::GetIsContextualTasksUpdateModeOnNavigationEnabled()) {
    InitializeInputStateModel();
  }
}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() {
  web_ui_interface_->SetComposeboxHandler(nullptr);
}

void ContextualTasksComposeboxHandler::MarkContextUploadFinished(
    const base::UnguessableToken& token) {
  pending_context_uploads_.erase(token);
  MaybeSendPendingQuery();
}

void ContextualTasksComposeboxHandler::MarkDelayedTabUploadFinished(
    const int32_t tab_id) {
  pending_delayed_tab_ids_.erase(tab_id);
  MaybeSendPendingQuery();
}

void ContextualTasksComposeboxHandler::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  // If the file token corresponds to the token uploaded via Lens when the
  // overlay is opened, then there is no need to do anything about the file
  // upload status.
  if (auto* controller = GetLensSearchController()) {
    if (controller->query_router() &&
        controller->query_router()->overlay_tab_context_file_token() ==
            context_token) {
      return;
    }
  }

  ContextualSearchboxHandler::OnContextUploadStatusChanged(
      context_token, mime_type, context_upload_status, error_type);
  // Associate tab with task.

  using ContextUploadStatus = contextual_search::ContextUploadStatus;
  bool is_terminal_upload_status =
      context_upload_status == ContextUploadStatus::kUploadSuccessful ||
      context_upload_status == ContextUploadStatus::kUploadFailed ||
      context_upload_status == ContextUploadStatus::kUploadExpired ||
      context_upload_status == ContextUploadStatus::kValidationFailed ||
      context_upload_status == ContextUploadStatus::kUploadReplaced;

  if (is_terminal_upload_status) {
    MarkContextUploadFinished(context_token);
  }
  if (context_upload_status == ContextUploadStatus::kUploadSuccessful) {
    auto* contextual_session_handle = GetContextualSessionHandle();
    if (!contextual_session_handle) {
      return;
    }

    const contextual_search::FileInfo* file_info =
        contextual_session_handle->GetController()->GetFileInfo(context_token);

    if (!file_info || !file_info->tab_session_id.has_value()) {
      return;
    }

    auto task_id = web_ui_interface_->GetTaskId();
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
  // Retrieve the overlay token before closing the overlay, as the controller
  // might be destroyed or reset during closure.
  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();
  bool has_visual_selection = overlay_token.has_value();
  auto* session_handle = GetContextualSessionHandle();

  // Every time a query is submitted, close the Lens overlay if it's open.
  CloseLensOverlay(
      lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
  std::optional<base::Uuid> task_id = web_ui_interface_->GetTaskId();
  auto* contextual_tasks_service = GetContextualTasksService();

  MaybeTriggerSmartTabSharingPromo(query,
                                   web_ui_interface_->GetWebUIWebContents());

  bool is_only_visual_selection =
      has_visual_selection && !IsAnyContextUploading() && session_handle &&
      session_handle->GetUploadedContextTokens().empty();
  if (!task_id.has_value() || !contextual_tasks_service ||
      is_only_visual_selection) {
    ContinueCreateAndSendQueryMessage(query, task_id, overlay_token);
    return;
  }

  std::vector<contextual_tasks::QueryContextualizer::TabId>
      tabs_to_recontextualize;
  // Get the active tab handle now, as recontextualization is an async process
  // and fetching the active tab later may result in unexpected behavior.
  tabs::TabInterface* active_tab = nullptr;
  auto* browser_window_interface = webui::GetBrowserWindowInterface(
      web_ui_interface_->GetWebUIWebContents());
  if (browser_window_interface) {
    TabListInterface* tab_list =
        TabListInterface::From(browser_window_interface);
    if (tab_list) {
      active_tab = tab_list->GetActiveTab();
      if (active_tab && !has_visual_selection) {
        tabs_to_recontextualize.push_back(active_tab->GetHandle().raw_value());
      }
    }

    auto* controller = contextual_tasks::ContextualTasksPanelController::From(
        browser_window_interface);
    if (controller && controller->IsPanelOpenForContextualTask()) {
      // Assume that if the panel is open for contextual tasks, the query is
      // being submitted from the side panel.
      controller->OnAiInteraction();
    }
  }

  std::vector<contextual_tasks::QueryContextualizer::TabId>
      tabs_to_force_contextualize;
  for (const auto& [token, tab_id] : delayed_tabs_) {
    tabs_to_force_contextualize.push_back(tab_id);
  }
  delayed_tabs_.clear();

  // Kick off the on-submit contextualization flow to upload delayed tabs and
  // recontextualize the active tab.
  recontextualization_pending_count_++;
  // It is safe to use base::Unretained(this) here because `recontextualizer_`
  // is owned by `this` and will be destroyed when `this` is destroyed,
  // cancelling any pending callbacks.
  recontextualizer_->Contextualize(
      task_id, query, tabs_to_recontextualize, tabs_to_force_contextualize,
      base::BindRepeating(
          &ContextualTasksComposeboxHandler::OnPageContextIneligible,
          base::Unretained(this)),
      base::BindRepeating(&ContextualTasksComposeboxHandler::
                              OnTabProcessedForQueryContextualization,
                          base::Unretained(this)),
      base::BindOnce(
          [](ContextualTasksComposeboxHandler* handler, std::string query,
             std::optional<base::Uuid> task_id,
             std::optional<base::UnguessableToken> token,
             base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
                 handle) {
            // The session handle is accessed via GetContextualSessionHandle(),
            // so we ignore it here.
            handler->ContinueCreateAndSendQueryMessage(query, task_id, token);
          },
          base::Unretained(this), query, task_id, overlay_token),
      IsSmartTabSharingActive());
}

contextual_tasks::ContextualTasksService*
ContextualTasksComposeboxHandler::GetContextualTasksService() {
  return contextual_tasks_service_;
}

void ContextualTasksComposeboxHandler::ResetInputStateModel() {
  ComposeboxHandler::ResetInputStateModel();
}

void ContextualTasksComposeboxHandler::UpdateModelFromUrl(const GURL& url) {
  if (input_state_model()) {
    input_state_model()->UpdateModelFromUrl(url);
  }
}

void ContextualTasksComposeboxHandler::OnTaskChanged() {
  ClearFiles(/*should_block_auto_suggested_tabs=*/false);
  InitializeInputStateModel();
}

void ContextualTasksComposeboxHandler::InitializeInputStateModel() {
  if (take_input_model_callback_) {
    std::unique_ptr<contextual_search::InputStateModel> current_input_state =
        take_input_model_callback_.Run();

    if (current_input_state) {
      ResetInputStateModel();
      input_state_model_ = std::move(current_input_state);

      input_state_subscription_ =
          input_state_model_->subscribe(base::BindRepeating(
              &ContextualTasksComposeboxHandler::OnInputStateChanged,
              weak_ptr_factory_.GetWeakPtr()));

      input_state_model_->Initialize();
    } else {
      ResetInputStateModel();
      ContextualSearchboxHandler::InitializeInputStateModel();
    }
  } else {
    ResetInputStateModel();
    ContextualSearchboxHandler::InitializeInputStateModel();
  }

  if (input_state_model_) {
    // crbug.com/488112121: Temporary implementation to disable file and deep
    // search when the aegc=1 URL parameter is present on the AI page.
    // This is moved from the WebUI to C++ to avoid extra Mojo APIs.
    GURL inner_frame_url = web_ui_interface_->GetInnerFrameUrl();
    std::string aegc_val;
    if (net::GetValueForKeyInQuery(inner_frame_url, "aegc", &aegc_val) &&
        aegc_val == "1") {
      input_state_model_->SetPermanentlyDisabledTools(
          {omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH});
      input_state_model_->SetPermanentlyDisabledInputTypes(
          {omnibox::InputType::INPUT_TYPE_LENS_FILE});
    }
  }
}

void ContextualTasksComposeboxHandler::AddFileContextFromBrowser(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    AddFileContextCallback callback) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  ContextualSearchboxHandler::page_->AddFileContext(token,
                                                    std::move(file_info));
  std::move(callback).Run(base::ok(token));
}

void ContextualTasksComposeboxHandler::OnPageContextIneligible() {
  web_ui_interface_->OnPageContextEligibilityChecked(false);
}

void ContextualTasksComposeboxHandler::OnTabProcessedForQueryContextualization(
    contextual_tasks::QueryContextualizer::TabId id) {
  pending_delayed_tab_ids_.erase(id);
}

void ContextualTasksComposeboxHandler::ContinueCreateAndSendQueryMessage(
    std::string query,
    std::optional<base::Uuid> original_task_id,
    std::optional<base::UnguessableToken> overlay_token) {
  if (recontextualization_pending_count_ > 0) {
    recontextualization_pending_count_--;
  }
  if (web_ui_interface_->GetTaskId() != original_task_id) {
    return;
  }
  // Create a client to aim message and send it to the page.
  if (auto* session_handle = GetContextualSessionHandle()) {
    // If there is an auto-added tab, the user sending the query means the
    // system should upload it.
    UploadSnapshotTabContextIfPresent();

    // Create a client to aim message and send it to the page.
    auto create_client_to_aim_request_info =
        contextual_tasks::PrepareClientToAimRequestInfo(
            query, session_handle, web_ui_interface_,
            GetInputState().active_tool, GetInputState().active_model,
            GetActiveTabContextId(), overlay_token);

    // Delay submission if context still uploading.
    if (IsAnyContextUploading()) {
      // Stash the request info instead of generating the message now.
      // Generating the message here would evaluate file upload statuses
      // prematurely, causing files that are still uploading to be stripped
      // from the message. Storing the request info allows us to generate the
      // message with up-to-date successful statuses once all uploads complete.
      pending_query_request_info_ =
          std::move(create_client_to_aim_request_info);
      return;
    }

    contextual_tasks::FinalizeAndSendAimQuery(
        std::move(create_client_to_aim_request_info), session_handle,
        web_ui_interface_);
  }
}

void ContextualTasksComposeboxHandler::HandleFileUpload(bool is_image) {
  content::WebContents* web_contents = web_ui_interface_->GetWebUIWebContents();
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
  } else if (!lens::features::IsLensSendRawFileMediaTypesEnabled()) {
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

bool ContextualTasksComposeboxHandler::IsAnyContextUploading() {
  return GetNumContextUploading() > 0 || GetNumTabsDelayed() > 0 ||
         recontextualization_pending_count_ > 0;
}

bool ContextualTasksComposeboxHandler::HasPendingQueryForTesting() const {
  return pending_query_request_info_ != nullptr;
}

uint16_t ContextualTasksComposeboxHandler::GetNumTabsDelayed() const {
  return static_cast<uint16_t>(pending_delayed_tab_ids_.size());
}

uint16_t ContextualTasksComposeboxHandler::GetNumContextUploading() const {
  return static_cast<uint16_t>(pending_context_uploads_.size());
}

void ContextualTasksComposeboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto* session_handle = GetContextualSessionHandle();
  if (!session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto token = session_handle->CreateContextToken();
  pending_context_uploads_.insert(token);
  std::string mime_type = file_info->mime_type;
  std::string file_name = file_info->file_name;
  ContextualSearchboxHandler::page_->AddFileContext(token,
                                                    std::move(file_info));
  std::move(callback).Run(base::ok(token));
  session_handle->StartFileContextUploadFlow(
      token, file_name, mime_type, std::move(file_bytes),
      ContextualSearchboxHandler::CreateImageEncodingOptions());
}

void ContextualTasksComposeboxHandler::FileSelectionCanceled() {
  file_dialog_.reset();
}

void ContextualTasksComposeboxHandler::AddTabContext(
    int32_t tab_id,
    bool delay_upload,
    AddTabContextCallback callback) {
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();

  // The delay_upload flag is used to indicate that the tab was auto-added
  // via the composebox. In the contextual-tasks case, added tabs should be
  // contextualized as late as possible so that the viewport and APC are
  // as recent as possible, put the tab in `delayed_tabs_` instead of using the
  // superclass method that contextualizes immediately and caches the tab
  // context for uploading in UploadSnapshotTabContextIfPresent.
  if (delay_upload) {
    // Because the superclass method is not called if delay_upload is true,
    // RecordTabAddedMetric() needs to be called here explicitly.
    if (tab) {
      RecordTabAddedMetric(tab, /*is_tab_suggestion_chip=*/true);
    }

    // Create a new token for the tab and add it to the `delayed_tabs_` map.
    // Do not use session handle's CreateContextToken() since the tab context
    // is not being uploaded yet.
    base::UnguessableToken token = base::UnguessableToken::Create();
    delayed_tabs_[token] = tab_id;
    pending_delayed_tab_ids_.insert(tab_id);
    std::move(callback).Run(base::ok(token));
    return;
  }

  // The tab was explicitly added by the user. Hence remove the URL from the
  // blocklist.
  if (tab) {
    web_ui_interface_->GetAutoSuggestionManager()->OnTabContextAdded(
        tab->GetContents()->GetLastCommittedURL(), tab->IsActivated());
  }

  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto token = contextual_session_handle->CreateContextToken();

  pending_context_uploads_.insert(token);

  ContextualSearchboxHandler::ContinueAddTabContext(tab_id, delay_upload, token,
                                                    std::move(callback));
}

void ContextualTasksComposeboxHandler::ClearFiles(
    bool should_block_auto_suggested_tabs) {
  // Clear all files from the UI.
  ComposeboxHandler::ClearFiles(should_block_auto_suggested_tabs);
  // Clear any delayed tabs.
  delayed_tabs_.clear();

  pending_delayed_tab_ids_.clear();
  pending_context_uploads_.clear();
  pending_query_request_info_.reset();
  visual_selection_token_.reset();
  visual_selection_overlay_token_.reset();

  if (should_block_auto_suggested_tabs) {
    web_ui_interface_->GetAutoSuggestionManager()->OnAutoSuggestionDismissed();
  }
}

void ContextualTasksComposeboxHandler::HandleLensButtonClick() {
  if (auto* controller = GetLensSearchController()) {
    if (controller->IsShowingUI()) {
      if (controller->invocation_source() ==
          lens::LensOverlayInvocationSource::kContextualTasksComposebox) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::
                kContextualTasksComposeboxLensButtonClick);
        return;
      } else {
        // If the overlay is showing from a different invocation source, clear
        // the selection and start fresh for a follow-up.
        if (controller->lens_overlay_controller()) {
          controller->lens_overlay_controller()->ClearAllSelections();
        }
        // Set the invocation source to contextual tasks so that any follow-up
        // queries are associated with the contextual tasks session via the
        // query flow router and thumbnails are added appropriately to the
        // composebox. This will work as if the overlay was opened from the
        // contextual tasks composebox in the first place.
        controller->SetInvocationSource(
            lens::LensOverlayInvocationSource::kContextualTasksComposebox);
      }
    }
    controller->SetThumbnailCreatedCallback(base::BindRepeating(
        &ContextualTasksComposeboxHandler::OnLensThumbnailCreated,
        weak_factory_.GetWeakPtr()));
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
    OnContextUploadStatusChanged(
        *visual_selection_token_, lens::MimeType::kImage,
        contextual_search::ContextUploadStatus::kUploadReplaced, std::nullopt);
  }

  // Lens will handle the creation of the interaction request needed for this
  // context. Add the visual selection to the composebox UI. The overlay token
  // is needed to ensure that the visual selection is associated with the
  // correct viewport upload.
  auto* controller = GetLensSearchController();
  CHECK(controller);
  CHECK(controller->query_router());
  CHECK(
      controller->query_router()->overlay_tab_context_file_token().has_value());
  base::UnguessableToken overlay_token =
      controller->query_router()->overlay_tab_context_file_token().value();
  AddFileContextFromBrowser(
      std::move(file_info),
      base::BindOnce(&ContextualTasksComposeboxHandler::OnVisualSelectionAdded,
                     weak_factory_.GetWeakPtr(), overlay_token));
}

// Only runs for non-delayed context. DeleteContext here runs
// ComposeboxHandler::DeleteContext.
void ContextualTasksComposeboxHandler::OnVisualSelectionAdded(
    base::UnguessableToken overlay_token,
    base::expected<base::UnguessableToken,
                   contextual_search::ContextUploadErrorType> token) {
  // Remove old visual selection if it exists.
  if (visual_selection_token_.has_value()) {
    ComposeboxHandler::DeleteContext(visual_selection_token_.value(),
                                     /*from_automatic_chip=*/false);
  }
  // Replace the visual selection token with the new one.
  if (token.has_value()) {
    visual_selection_token_ = token.value();
    // The overlay token needs to be stored along with the visual selection
    // token so that it can be used for the query even if the overlay is closed
    // and reopened.
    visual_selection_overlay_token_ = overlay_token;

    // Since a fake visual selection file is added to the composebox for the
    // purpose of UI representation, this needs to call the
    // OnContextUploadStatusChanged() to avoid the visual selection being
    // considered as pending upload. Assume it is kUploadSuccessful.
    OnContextUploadStatusChanged(
        *visual_selection_token_, lens::MimeType::kImage,
        contextual_search::ContextUploadStatus::kUploadSuccessful,
        std::nullopt);
  } else {
    visual_selection_token_ = std::nullopt;
    visual_selection_overlay_token_ = std::nullopt;
  }
}

void ContextualTasksComposeboxHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {
  // Get the URL associated with the chip before deletion. If it's not an
  // auto-suggest chip the tab info should be already added to the session
  // handle. For auto-suggest chips, session handle isn't yet aware of it. So we
  // will get the active tab's URL.
  std::optional<GURL> deleted_tab_url;
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    const contextual_search::FileInfo* file_info =
        contextual_session_handle->GetController()->GetFileInfo(file_token);
    if (file_info) {
      deleted_tab_url = file_info->tab_url;
      auto injected_input_id = file_info->GetInjectedInputId();
      if (injected_input_id.has_value()) {
        contextual_tasks::SendInjectedInputRemovedUpdate(
            web_ui_interface_, injected_input_id.value());
      }
    }
  }

  auto it = delayed_tabs_.find(file_token);
  bool was_delayed = it != delayed_tabs_.end();
  if (was_delayed) {                           // Delayed tab:
    MarkDelayedTabUploadFinished(it->second);  // tab id.
    delayed_tabs_.erase(it);
  } else {  // File/normal context:
    ComposeboxHandler::DeleteContext(file_token, from_automatic_chip);
    MarkContextUploadFinished(file_token);
  }

  // Clear the visual selection token if it matches the deleted token.
  if (visual_selection_token_ && *visual_selection_token_ == file_token) {
    visual_selection_token_ = std::nullopt;
    visual_selection_overlay_token_ = std::nullopt;
    // If the user explicitly deleted the context (not from automatic chip),
    // close the Lens Overlay.
    if (!from_automatic_chip) {
      if (auto* controller = GetLensSearchController()) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::kContextualTasksContextCleared);
      }
    }
  }
  if (was_delayed) {
    OnContextUploadStatusChanged(
        file_token, lens::MimeType::kUnknown,
        contextual_search::ContextUploadStatus::kUploadExpired, std::nullopt);
  }

  // Hide the underline for the tab if it was associated with the deleted
  // context.
  auto* browser_window_interface = webui::GetBrowserWindowInterface(
      web_ui_interface_->GetWebUIWebContents());
  auto* active_task_context_provider =
      browser_window_interface
          ? contextual_tasks::ActiveTaskContextProvider::From(
                browser_window_interface)
          : nullptr;
  if (active_task_context_provider) {
    active_task_context_provider->RefreshContext();
  }

  auto* auto_suggestion_manager = web_ui_interface_->GetAutoSuggestionManager();
  if (from_automatic_chip) {
    auto_suggestion_manager->OnAutoSuggestionDismissed();
  } else if (deleted_tab_url.has_value()) {
    auto_suggestion_manager->OnTabContextRemoved(deleted_tab_url.value());
  }
}

void ContextualTasksComposeboxHandler::UpdateSuggestedTabContext(
    const contextual_tasks::SuggestedTabInfo* suggested_tab) {
  // Always use the passed info as the result of the manager's filtering.
  searchbox::mojom::TabInfoPtr filtered_suggestion;
  const bool is_tab_suggestion_enabled =
      contextual_tasks::GetIsTabAutoSuggestionChipEnabled() ||
      (suggested_tab && ShouldForceAllowTabSuggestion(suggested_tab->tab_id));

  if (is_tab_suggestion_enabled && suggested_tab) {
    filtered_suggestion = searchbox::mojom::TabInfo::New();
    filtered_suggestion->tab_id = suggested_tab->tab_id;
    filtered_suggestion->title = base::UTF16ToUTF8(suggested_tab->title);
    filtered_suggestion->url = suggested_tab->url;
    filtered_suggestion->last_active = suggested_tab->last_active;
  }

  SearchboxHandler::page_->UpdateAutoSuggestedTabContext(
      std::move(filtered_suggestion));
}

void ContextualTasksComposeboxHandler::CloseLensOverlay(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensSync(dismissal_source);
  }
}

void ContextualTasksComposeboxHandler::CloseLensOverlayFromWebUI(
    composebox::mojom::LensOverlayDismissalSource dismissal_source) {
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensAsync(ToLensOverlayDismissalSource(dismissal_source));
  }
}

LensSearchController*
ContextualTasksComposeboxHandler::GetLensSearchController() const {
  auto* browser = web_ui_interface_->GetBrowser();
  if (!browser) {
    return nullptr;
  }

  if (auto* controller = LensSearchController::FromTabWebContents(
          TabListInterface::From(browser)->GetActiveTab()->GetContents());
      controller) {
    return controller;
  }
  return nullptr;
}

std::optional<base::UnguessableToken>
ContextualTasksComposeboxHandler::GetLensOverlayToken() {
  // If there is a visual selection token in the composebox, then the overlay
  // token should be returned to ensure the AIM request is correctly
  // constructed with the overlay context.
  if (visual_selection_token_.has_value()) {
    visual_selection_token_.reset();
    return visual_selection_overlay_token_;
  }

  if (auto* controller = GetLensSearchController()) {
    // If there is no region selection, then do not return the overlay token.
    // This is needed to prevent the token from being used in the client to aim
    // request when a new overlay was opened and no region was selected.
    if (!controller->lens_overlay_controller()->HasRegionSelection()) {
      return std::nullopt;
    }

    if (auto* router = controller->query_router()) {
      return router->overlay_tab_context_file_token();
    }
  }
  return std::nullopt;
}

std::optional<int64_t>
ContextualTasksComposeboxHandler::GetActiveTabContextId() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    return std::nullopt;
  }

  auto* browser_window_interface = webui::GetBrowserWindowInterface(
      web_ui_interface_->GetWebUIWebContents());
  if (!browser_window_interface) {
    return std::nullopt;
  }
  TabListInterface* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    return std::nullopt;
  }
  auto* active_tab = tab_list->GetActiveTab();
  if (!active_tab) {
    return std::nullopt;
  }
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  if (!active_tab_id.is_valid()) {
    return std::nullopt;
  }

  auto file_infos = contextual_session_handle->GetUploadedContextFileInfos();
  auto submitted_file_infos =
      contextual_session_handle->GetSubmittedContextFileInfos();
  file_infos.insert(file_infos.end(), submitted_file_infos.begin(),
                    submitted_file_infos.end());
  for (const auto& file_info : file_infos) {
    if (file_info.tab_session_id &&
        file_info.tab_session_id->id() == active_tab_id.id()) {
      return file_info.GetContextId();
    }
  }
  return std::nullopt;
}

bool ContextualTasksComposeboxHandler::ShouldForceAllowTabSuggestion(
    int32_t tab_id) {
  auto* browser = web_ui_interface_->GetBrowser();
  if (!browser) {
    return false;
  }
  auto* active_tab = TabListInterface::From(browser)->GetActiveTab();
  if (!active_tab) {
    return false;
  }

  // If the tab being evaluated is not the currently active tab, ignore it.
  if (active_tab->GetHandle().raw_value() != tab_id) {
    return false;
  }

  auto* session_handle = GetContextualSessionHandle();
  if (!session_handle) {
    return false;
  }

  return session_handle->is_contextual_lens_session();
}

void ContextualTasksComposeboxHandler::MaybeSendPendingQuery() {
  if (pending_query_request_info_ && !IsAnyContextUploading()) {
    auto* session_handle = GetContextualSessionHandle();
    if (session_handle) {
      contextual_tasks::FinalizeAndSendAimQuery(
          std::move(pending_query_request_info_), session_handle,
          web_ui_interface_);
    }
    pending_query_request_info_.reset();
  }
}
