// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
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
        pending_searchbox_handler)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents,
                                                             this))),
      web_ui_controller_(ui_controller) {}

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
  // Create a client to aim message and send it to the page.
  if (auto* contextual_search_web_contents_helper =
          ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
              web_ui_controller_->GetWebUIWebContents());
      contextual_search_web_contents_helper->session_handle()) {
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
    lens::ClientToAimMessage client_to_page_message =
        contextual_search_web_contents_helper->session_handle()
            ->CreateClientToAimRequest(
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
  auto* contextual_search_web_contents_helper =
      ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
          web_ui_controller_->GetWebUIWebContents());

  if (contextual_search_web_contents_helper &&
      contextual_search_web_contents_helper->session_handle()) {
    std::string mime_type = file_info->mime_type;
    contextual_search_web_contents_helper->session_handle()->AddFileContext(
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
