// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/lens/lens_url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/modality_chip_props.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#endif

namespace {
constexpr char kMyActivityUrl[] = "https://myactivity.google.com/myactivity";

void OpenUrlWithDisposition(Profile* profile,
                            const GURL& url,
                            WindowOpenDisposition disposition) {
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  Navigate(&params);
}

std::vector<contextual_tasks::mojom::ContextInfoPtr>
PopulateContextualResources(contextual_tasks::ContextualTaskContext* context) {
  if (!context) {
    return {};
  }
  std::vector<contextual_tasks::mojom::ContextInfoPtr> context_items;
  for (const auto& attachment : context->GetUniqueUrlAttachments()) {
    const GURL url = attachment.GetURL();
    const std::string title = base::UTF16ToUTF8(attachment.GetTitle());

    // Skip if the title is empty. Empty URLs are right now allowed for PDF /
    // images.
    if (title.empty() ||
        (!url.is_valid() && attachment.GetResourceType() ==
                                contextual_tasks::ResourceType::kWebpage)) {
      continue;
    }

    switch (attachment.GetResourceType()) {
      case contextual_tasks::ResourceType::kWebpage: {
        auto tab_context = contextual_tasks::mojom::TabContext::New();
        tab_context->title = title;
        tab_context->url = url;
        tab_context->tab_id = attachment.GetTabSessionId().id();
        context_items.push_back(contextual_tasks::mojom::ContextInfo::NewTab(
            std::move(tab_context)));
        break;
      }
      case contextual_tasks::ResourceType::kPdf: {
        auto file_context = contextual_tasks::mojom::FileContext::New();
        file_context->title = title;
        file_context->url = url;
        context_items.push_back(contextual_tasks::mojom::ContextInfo::NewFile(
            std::move(file_context)));
        break;
      }
      case contextual_tasks::ResourceType::kImage: {
        auto image_context = contextual_tasks::mojom::ImageContext::New();
        image_context->title = title;
        image_context->url = url;
        context_items.push_back(contextual_tasks::mojom::ContextInfo::NewImage(
            std::move(image_context)));
        break;
      }
      case contextual_tasks::ResourceType::kUnknown:
        break;
    }
  }
  return context_items;
}

}  // namespace

namespace contextual_tasks {
contextual_tasks::mojom::ComposeboxPositionPtr InputPlateConfigToMojo(
    const lens::InputPlateParametersRequest& update_msg) {
  auto mojo_position = contextual_tasks::mojom::ComposeboxPosition::New();

  if (update_msg.has_max_width()) {
    // AIM Proto is int32 since some languages in Google3 do not handle
    // uint32. If we have a negative value, we set it as 0 since width
    // cannot be negative. Mojom is uint32 for proper representation
    // and security concerns.
    if (update_msg.max_width() < 0) {
      mojo_position->max_width = 0;
    } else {
      mojo_position->max_width = static_cast<uint32_t>(update_msg.max_width());
    }
  }
  if (update_msg.has_max_height()) {
    // AIM Proto is int32 since some languages in Google3 do not handle
    // uint32. If we have a negative value, we set it as 0 since height
    // cannot be negative. Mojom is uint32 for proper representation
    // and security concerns.
    if (update_msg.max_height() < 0) {
      mojo_position->max_height = 0;
    } else {
      mojo_position->max_height =
          static_cast<uint32_t>(update_msg.max_height());
    }
  }
  if (update_msg.has_margin_bottom()) {
    mojo_position->margin_bottom = update_msg.margin_bottom();
  }
  if (update_msg.has_margin_left()) {
    mojo_position->margin_left = update_msg.margin_left();
  }
  return mojo_position;
}
}  // namespace contextual_tasks

ContextualTasksPageHandler::ContextualTasksPageHandler(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
    contextual_tasks::ContextualTasksUIInterface* web_ui_controller,
    contextual_tasks::ContextualTasksUiService* ui_service,
    contextual_tasks::ContextualTasksService* contextual_tasks_service)
    : receiver_(this, std::move(receiver)),
      web_ui_controller_(web_ui_controller),
      ui_service_(ui_service),
      contextual_tasks_service_(contextual_tasks_service) {
  CHECK(contextual_tasks_service_);
  contextual_tasks_service_observation_.Observe(contextual_tasks_service_);
}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  std::optional<base::Uuid> task_id = web_ui_controller_->GetTaskId();
  if (task_id.has_value()) {
    std::move(callback).Run(
        ui_service_->GetDefaultAiPageUrlForTask(task_id.value()));
    return;
  }
  std::move(callback).Run(ui_service_->GetDefaultAiPageUrl());
}

void ContextualTasksPageHandler::GetUrlForTask(const base::Uuid& uuid,
                                               GetUrlForTaskCallback callback) {
  // First check if there's an initial URL.
  std::optional<GURL> initial_url = ui_service_->GetInitialUrlForTask(uuid);
  if (initial_url) {
    std::move(callback).Run(initial_url.value());
    return;
  }

  GURL aim_url = web_ui_controller_->GetAimUrl();
  if (!aim_url.is_empty()) {
    std::move(callback).Run(aim_url);
    return;
  }

  // There's a slight difference in the callback signature between the mojo
  // api (wants a reference) and the ui service (provided a moved object).
  // The latter can't provide a reference since we're not keeping it
  // long-term, hence wrapping this in a base::BindOnce.
  ui_service_->GetThreadUrlFromTaskId(
      uuid, base::BindOnce([](GetUrlForTaskCallback callback,
                              GURL url) { std::move(callback).Run(url); },
                           std::move(callback)));
}

void ContextualTasksPageHandler::SetTaskId(const base::Uuid& uuid) {
  web_ui_controller_->SetTaskId(uuid);

  // Trigger an update to the UI with the initial set of tabs for this task.
  UpdateContextForTask(uuid);
}

void ContextualTasksPageHandler::SetThreadTitle(const std::string& title) {
  web_ui_controller_->SetThreadTitle(title);
}

void ContextualTasksPageHandler::IsZeroState(const GURL& url,
                                             IsZeroStateCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(ContextualTasksUI::IsZeroState(url, ui_service_));
#else
  std::move(callback).Run(false);
#endif
}

void ContextualTasksPageHandler::IsAiPage(const GURL& url,
                                          IsAiPageCallback callback) {
  std::move(callback).Run(ui_service_->IsAiUrl(url));
}

void ContextualTasksPageHandler::IsPendingErrorPage(
    const base::Uuid& task_id,
    IsPendingErrorPageCallback callback) {
  std::move(callback).Run(ui_service_->IsPendingErrorPage(task_id));
}

void ContextualTasksPageHandler::CloseSidePanel() {
  web_ui_controller_->CloseSidePanel();
}

void ContextualTasksPageHandler::ShowThreadHistory() {
  // Send a message to AIM to open the threads view.
  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();
  PostMessageToWebview(message);
}

void ContextualTasksPageHandler::IsShownInTab(IsShownInTabCallback callback) {
  std::move(callback).Run(web_ui_controller_->IsShownInTab());
}

void ContextualTasksPageHandler::OpenMyActivityUi() {
  OpenUrlWithDisposition(web_ui_controller_->GetProfile(), GURL(kMyActivityUrl),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void ContextualTasksPageHandler::OpenHelpUi() {
  if (skip_feedback_ui_for_testing_) {
    return;
  }
  GURL page_url =
      web_ui_controller_->GetWebUIWebContents()->GetLastCommittedURL();
  if (auto* browser = web_ui_controller_->GetBrowser()) {
    if (auto* tab_list = TabListInterface::From(browser)) {
      if (auto* active_tab = tab_list->GetActiveTab()) {
        page_url = active_tab->GetContents()->GetLastCommittedURL();
      }
    }
  }
  chrome::ShowFeedbackPage(page_url, web_ui_controller_->GetProfile(),
                           feedback::kFeedbackSourceAI,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/
                           l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK),
                           /*category_tag=*/"cobrowse",
                           /*extra_diagnostics=*/std::string());
}

void ContextualTasksPageHandler::OpenOnboardingHelpUi() {
  OpenUrlWithDisposition(
      web_ui_controller_->GetProfile(),
      GURL(contextual_tasks::GetContextualTasksOnboardingTooltipHelpUrl()),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void ContextualTasksPageHandler::OpenUrl(const GURL& url,
                                         WindowOpenDisposition disposition) {
  OpenUrlWithDisposition(web_ui_controller_->GetProfile(), url, disposition);
}

void ContextualTasksPageHandler::MoveTaskUiToNewTab() {
  auto* browser = web_ui_controller_->GetBrowser();
  const auto& task_id = web_ui_controller_->GetTaskId();
  if (!task_id.has_value()) {
    LOG(ERROR) << "Attempted to open in new tab with no valid task ID.";
    return;
  }

  ui_service_->MoveTaskUiToNewTab(task_id.value(), browser,
                                  web_ui_controller_->GetInnerFrameUrl());
}

void ContextualTasksPageHandler::OnTabClickedFromSourcesMenu(int32_t tab_id,
                                                             const GURL& url) {
  if (ui_service_) {
    ui_service_->OnTabClickedFromSourcesMenu(tab_id, url,
                                             web_ui_controller_->GetBrowser());
  }
}

void ContextualTasksPageHandler::OnFileClickedFromSourcesMenu(const GURL& url) {
  if (ui_service_) {
    ui_service_->OnFileClickedFromSourcesMenu(url,
                                              web_ui_controller_->GetBrowser());
  }
}

void ContextualTasksPageHandler::OnImageClickedFromSourcesMenu(
    const GURL& url) {
  if (ui_service_) {
    ui_service_->OnImageClickedFromSourcesMenu(
        url, web_ui_controller_->GetBrowser());
  }
}

void ContextualTasksPageHandler::OnWebviewMessage(
    const std::vector<uint8_t>& message) {
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    web_ui_controller_->GetPageRemote()->OnHandshakeComplete();
    web_ui_controller_->OnSidePanelStateChanged();
  } else if (aim_to_client_message.has_hide_input()) {
    web_ui_controller_->GetPageRemote()->HideInput();
  } else if (aim_to_client_message.has_restore_input()) {
    web_ui_controller_->GetPageRemote()->RestoreInput();
  } else if (aim_to_client_message.has_enter_basic_mode()) {
    web_ui_controller_->GetPageRemote()->HideInput();
  } else if (aim_to_client_message
                 .has_set_chrome_desktop_input_plate_configuration()) {
    const auto& update_msg =
        aim_to_client_message.set_chrome_desktop_input_plate_configuration();

    auto mojo_position = contextual_tasks::InputPlateConfigToMojo(update_msg);

    web_ui_controller_->GetPageRemote()->UpdateComposeboxPosition(
        std::move(mojo_position));
  } else if (aim_to_client_message.has_exit_basic_mode()) {
    web_ui_controller_->GetPageRemote()->RestoreInput();
  } else if (aim_to_client_message.has_update_thread_context_library()) {
    OnReceivedUpdatedThreadContextLibrary(
        aim_to_client_message.update_thread_context_library());
  } else if (aim_to_client_message.has_notify_zero_state_rendered() &&
             base::FeatureList::IsEnabled(
                 contextual_tasks::kEnableNotifyZeroStateRenderedCapability)) {
    web_ui_controller_->OnZeroStateChange(
        aim_to_client_message.notify_zero_state_rendered()
            .is_zero_state_rendered());
  } else if (aim_to_client_message.has_inject_input()) {
    OnReceivedInjectInput(std::make_unique<lens::ModalityChipProps>(
        aim_to_client_message.inject_input().modality()));
  } else if (aim_to_client_message.has_remove_injected_input()) {
    OnReceivedRemoveInjectedInput(
        std::string(aim_to_client_message.remove_injected_input().id()));
  } else if (aim_to_client_message.has_lock_input()) {
    web_ui_controller_->GetPageRemote()->LockInput();
  } else if (aim_to_client_message.has_unlock_input()) {
    web_ui_controller_->GetPageRemote()->UnlockInput();
  }
}

void ContextualTasksPageHandler::GetCommonSearchParams(
    bool is_dark_mode,
    bool is_side_panel,
    GetCommonSearchParamsCallback callback) {
  // The server is not yet ready to adapt the side panel UI unless the gsc=2
  // param is set. So force side panel mode if the temporary feature flag is
  // enabled.
  if (contextual_tasks::ShouldForceGscInTabMode()) {
    is_side_panel = true;
  }

  std::string country_code =
      g_browser_process->GetFeatures()->application_locale_storage()->Get();

  if (contextual_tasks::ShouldForceCountryCodeUS()) {
    country_code = "US";
  }

  auto params = lens::GetCommonSearchParametersMap(country_code, is_dark_mode,
                                                   is_side_panel);
  if (contextual_tasks::ShouldForceCountryCodeUS()) {
    params["gl"] = "us";
  }
  std::move(callback).Run(
      base::flat_map<std::string, std::string>(params.begin(), params.end()));
}

void ContextualTasksPageHandler::OnboardingTooltipDismissed() {
  Profile* profile = web_ui_controller_->GetProfile();
  if (!profile) {
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }

  int count = prefs->GetInteger(
      contextual_tasks::kContextualTasksOnboardingTooltipDismissedCount);
  prefs->SetInteger(
      contextual_tasks::kContextualTasksOnboardingTooltipDismissedCount,
      count + 1);
}

void ContextualTasksPageHandler::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  DCHECK(web_ui_controller_->GetPageRemote());
  if (!web_ui_controller_->GetPageRemote()) {
    return;
  }

  const size_t size = message.ByteSizeLong();
  if (size == 0) {
    LOG(WARNING) << "PostMessageToWebview called with an empty message.";
    return;
  }
  std::vector<uint8_t> serialized_message(size);
  if (!message.SerializeToArray(&serialized_message[0], size)) {
    LOG(ERROR) << "Failed to serialize ClientToAimMessage.";
    return;
  }

  web_ui_controller_->GetPageRemote()->PostMessageToWebview(serialized_message);
}

void ContextualTasksPageHandler::OnTaskAdded(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  if (!web_ui_controller_->GetPageRemote()) {
    return;
  }

  UpdateContextForTask(task.GetTaskId());
}

void ContextualTasksPageHandler::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  if (!web_ui_controller_->GetPageRemote()) {
    return;
  }

  const auto& current_task_id = web_ui_controller_->GetTaskId();
  if (current_task_id != task.GetTaskId()) {
    return;
  }

  UpdateContextForTask(task.GetTaskId());
}

void ContextualTasksPageHandler::UpdateContextForTask(
    const base::Uuid& task_id) {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksContextLibrary)) {
    web_ui_controller_->GetPageRemote()->OnContextUpdated({});
    return;
  }
  contextual_tasks_service_->GetContextForTask(
      task_id, {contextual_tasks::ContextualTaskContextSource::kTabStrip},
      std::make_unique<contextual_tasks::ContextDecorationParams>(),
      base::BindOnce(
          [](base::WeakPtr<ContextualTasksPageHandler> self,
             std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
            if (self && self->web_ui_controller_->GetPageRemote()) {
              std::vector<contextual_tasks::mojom::ContextInfoPtr>
                  context_items = PopulateContextualResources(context.get());
              self->web_ui_controller_->GetPageRemote()->OnContextUpdated(
                  std::move(context_items));
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void ContextualTasksPageHandler::OnReceivedUpdatedThreadContextLibrary(
    const lens::UpdateThreadContextLibrary& message) {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksContextLibrary)) {
    return;
  }
  const auto& task_id = web_ui_controller_->GetTaskId();
  if (!task_id.has_value()) {
    return;
  }

  contextual_search::ContextualSearchSessionHandle* handle =
      web_ui_controller_->GetOrCreateContextualSessionHandle();

  std::vector<contextual_search::FileInfo> submitted_context;
  if (handle) {
    submitted_context = handle->GetSubmittedContextFileInfos();
    // Now that we have extracted the submitted contexts and are ready to update
    // the context in the ContextualTask, we can clear out the submitted context
    // from the ContextualSearchSessionHandle.
    handle->ClearSubmittedContextTokens();
  }

  std::vector<contextual_tasks::UrlResource> committed_context =
      contextual_tasks::ConvertAiModeContextToUrlResources(message,
                                                           submitted_context);
  contextual_tasks_service_->SetUrlResourcesFromServer(*task_id,
                                                       committed_context);
}

void ContextualTasksPageHandler::OnReceivedInjectInput(
    std::unique_ptr<lens::ModalityChipProps> modality) {
  contextual_search::ContextualSearchSessionHandle* handle =
      web_ui_controller_->GetOrCreateContextualSessionHandle();
  auto token = handle->CreateContextToken();
  web_ui_controller_->GetPageRemote()->InjectInput(
      std::string(modality->title()), std::string(modality->thumbnail_src()),
      token);
  // This does not actually upload anything, but allows the injected input to be
  // shown in the chip carousel in the UI.
  handle->StartModalityChipUploadFlow(token, std::move(modality));
}

void ContextualTasksPageHandler::OnReceivedRemoveInjectedInput(
    const std::string& id) {
  contextual_search::ContextualSearchSessionHandle* handle =
      web_ui_controller_->GetOrCreateContextualSessionHandle();
  auto token = handle->GetController()->FindTokenForInjectedInput(id);
  if (token.has_value()) {
    web_ui_controller_->GetPageRemote()->RemoveInjectedInput(token.value());
  }
}
