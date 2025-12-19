// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/tabs/public/tab_interface.h"
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
      context_controller_(
          contextual_tasks::ContextualTasksContextControllerFactory::
              GetForProfile(profile)) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualOmniboxClient*>(omnibox_controller()->client())
      ->SetSuggestInputsCallback(base::BindRepeating(
          &ContextualTasksComposeboxHandler::GetSuggestInputs,
          base::Unretained(this)));
}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  CreateAndSendQueryMessage(query_text);
}

void ContextualTasksComposeboxHandler::CreateAndSendQueryMessage(
    const std::string& query) {
  std::optional<base::Uuid> task_id = web_ui_controller_->GetTaskId();
  auto* context_controller = GetContextController();
  if (!task_id.has_value() || !context_controller) {
    ContinueCreateAndSendQueryMessage(query);
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

  // TODO(crbug.com/468453630): The context needs to actually be populated
  // with tab data from the server-managed context list.
  context_controller->GetContextForTask(
      *task_id, {}, nullptr,
      base::BindOnce(&ContextualTasksComposeboxHandler::OnContextRetrieved,
                     weak_factory_.GetWeakPtr(), query, active_tab_handle));
}

contextual_tasks::ContextualTasksContextController*
ContextualTasksComposeboxHandler::GetContextController() {
  return context_controller_;
}

void ContextualTasksComposeboxHandler::OnContextRetrieved(
    std::string query,
    tabs::TabHandle active_tab_handle,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
  if (!context) {
    ContinueCreateAndSendQueryMessage(query);
    return;
  }

  tabs::TabInterface* active_tab = active_tab_handle.Get();
  std::vector<tabs::TabInterface*> tabs_to_update =
      GetTabsToUpdate(*context, active_tab);

  if (tabs_to_update.empty()) {
    ContinueCreateAndSendQueryMessage(query);
    return;
  }

  // Use a barrier closure to wait for all tabs to be processed.
  // The callback will be invoked once for each tab.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      tabs_to_update.size(),
      base::BindOnce(
          &ContextualTasksComposeboxHandler::ContinueCreateAndSendQueryMessage,
          weak_factory_.GetWeakPtr(), query));

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
        barrier_closure, tab_id));
  }
}

void ContextualTasksComposeboxHandler::OnTabContextualizationFetched(
    std::string query,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context,
    base::RepeatingClosure barrier_closure,
    int32_t tab_id,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data) {
    barrier_closure.Run();
    return;
  }

  // TODO(crbug.com/468431232): Add logic to check if the content has actually
  // changed significantly enough to warrant a re-upload. For now, always
  // re-upload.
  std::optional<int64_t> maybe_context_id = std::nullopt;
  if (page_content_data->tab_session_id.has_value()) {
    maybe_context_id =
        GetContextIdForTab(*context, page_content_data->tab_session_id.value());
  }

  UploadTabContextWithData(
      tab_id, maybe_context_id, std::move(page_content_data),
      base::BindOnce(&ContextualTasksComposeboxHandler::OnTabContextReuploaded,
                     weak_factory_.GetWeakPtr(), query, barrier_closure));
}

void ContextualTasksComposeboxHandler::OnTabContextReuploaded(
    std::string query,
    base::RepeatingClosure barrier_closure,
    bool success) {
  barrier_closure.Run();
}

std::vector<tabs::TabInterface*>
ContextualTasksComposeboxHandler::GetTabsToUpdate(
    const contextual_tasks::ContextualTaskContext& context,
    tabs::TabInterface* active_tab) {
  std::vector<tabs::TabInterface*> tabs_to_update;

  // TODO(crbug.com/468430623): Support updating multiple tabs.
  // Currently this only checks the active tab.
  if (!active_tab) {
    return tabs_to_update;
  }

  tabs_to_update.push_back(active_tab);

  return tabs_to_update;
}

std::optional<int64_t> ContextualTasksComposeboxHandler::GetContextIdForTab(
    const contextual_tasks::ContextualTaskContext& context,
    SessionID tab_session_id) {
  auto* controller = GetContextController();
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

  for (const auto& attachment : context.GetUrlAttachments()) {
    // Currently, the only field of the url attachment that is used to determine
    // if the attachment is from the context is if the tab session id is
    // present.
    if (attachment.GetTabSessionId() == tab_session_id) {
      const auto& file_info_list = search_context_controller->GetFileInfoList();
      for (const auto* file_info : file_info_list) {
        if (file_info->tab_session_id == tab_session_id) {
          return file_info->GetContextId();
        }
      }
      break;
    }
  }
  return std::nullopt;
}

void ContextualTasksComposeboxHandler::ContinueCreateAndSendQueryMessage(
    std::string query) {
  // Create a client to aim message and send it to the page.
  if (auto* session_handle = GetContextualSessionHandle()) {
    // If there is an auto-added tab, the user sending the query means we should
    // upload it.
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

void ContextualTasksComposeboxHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {
  ComposeboxHandler::DeleteContext(file_token, from_automatic_chip);
  if (from_automatic_chip) {
    web_ui_controller_->DisableActiveTabContextSuggestion();
  }
}
