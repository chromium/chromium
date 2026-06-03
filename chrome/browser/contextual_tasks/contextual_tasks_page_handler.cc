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
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom-shared.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/common/pref_names.h"
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
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/logger.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/aim_icon.pb.h"
#include "third_party/lens_server_proto/modality_chip_props.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#endif

namespace {
constexpr char kMyActivityUrl[] = "https://myactivity.google.com/myactivity";

void OpenUrlWithDisposition(Profile* profile,
                            const GURL& url,
                            WindowOpenDisposition disposition,
                            BrowserWindowInterface* browser) {
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  params.browser = browser;
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

contextual_tasks::mojom::IconType IconTypeToMojom(lens::AimIconType icon_id) {
  switch (icon_id) {
    case lens::AimIconType::ICON_TYPE_ADD:
      return contextual_tasks::mojom::IconType::kAdd;
    case lens::AimIconType::ICON_TYPE_CHECK:
      return contextual_tasks::mojom::IconType::kCheck;
    case lens::AimIconType::ICON_TYPE_FORMAT_QUOTE_FILLED:
      return contextual_tasks::mojom::IconType::kFormatQuoteFilled;
    case lens::AimIconType::ICON_TYPE_INVERTED_FORMAT_QUOTE_FILLED:
      return contextual_tasks::mojom::IconType::kInvertedFormatQuoteFilled;
    case lens::AimIconType::ICON_TYPE_IMAGE:
      return contextual_tasks::mojom::IconType::kImage;
    case lens::AimIconType::ICON_TYPE_DRIVE_PDF:
      return contextual_tasks::mojom::IconType::kDrivePdf;
    default:
      return contextual_tasks::mojom::IconType::kUnspecified;
  }
}

// Returns the active (uploaded but not submitted) context token that matches
// the provided injected input ID. This specifically iterates over the active
// token set rather than using FindTokenForInjectedInput to ensure that
// operations only target un-submitted inputs currently present in the
// composebox, filtering out artifacts from previous queries stored in the
// controller map.
std::optional<base::UnguessableToken> FindActiveInjectedInputToken(
    contextual_search::ContextualSearchSessionHandle* handle,
    std::string_view id) {
  if (!handle || !handle->GetController() || id.empty()) {
    return std::nullopt;
  }
  for (const auto& token : handle->GetUploadedContextTokens()) {
    const auto* file_info = handle->GetController()->GetFileInfo(token);
    if (file_info) {
      auto injected_id = file_info->GetInjectedInputId();
      if (injected_id.has_value() && injected_id.value() == id) {
        return token;
      }
    }
  }
  return std::nullopt;
}

#if !BUILDFLAG(IS_ANDROID)
int GetSmartTabSharingFeatureActivationCount(
    feature_engagement::Tracker* tracker) {
  if (!tracker) {
    return 0;
  }
  for (const auto& [config, count] : tracker->ListEvents(
           feature_engagement::kIPHSmartTabSharingDefaultOnFeature)) {
    if (config.name == "smart_tab_sharing_activated") {
      return count;
    }
  }
  return 0;
}
#endif

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
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    contextual_tasks::ContextualTasksPanelController* panel_controller)
    : receiver_(this, std::move(receiver)),
      web_ui_controller_(web_ui_controller),
      ui_service_(ui_service),
      contextual_tasks_service_(contextual_tasks_service),
      panel_controller_(panel_controller) {
  CHECK(contextual_tasks_service_);
  contextual_tasks_service_observation_.Observe(contextual_tasks_service_);

  if (contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled()) {
    Profile* profile = web_ui_controller_->GetProfile();
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kPinContextualTaskButton,
        base::BindRepeating(&ContextualTasksPageHandler::OnPrefChanged,
                            base::Unretained(this)));
    OnPrefChanged();
  }
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
  // Wrap the callback to ensure previous_query is set on the session handle
  // regardless of whether the URL is returned synchronously or asynchronously.
  auto wrapped_callback = base::BindOnce(
      [](base::WeakPtr<ContextualTasksPageHandler> self,
         GetUrlForTaskCallback original_callback, const GURL& url) {
        if (self && self->web_ui_controller_) {
          if (auto* session_handle =
                  self->web_ui_controller_
                      ->GetOrCreateContextualSessionHandle()) {
            std::string query = lens::ExtractTextQueryParameterValue(url);
            if (!query.empty()) {
              session_handle->set_previous_query(query);
            }
          }
        }
        std::move(original_callback).Run(url);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // First check if there's an initial URL.
  std::optional<GURL> initial_url = ui_service_->GetInitialUrlForTask(uuid);
  if (initial_url) {
    std::move(wrapped_callback)
        .Run(contextual_tasks::ContextualTasksUiService::CopyParamsFromWebUIUrl(
            initial_url.value(), web_ui_controller_->GetWebUiUrl()));
    return;
  }

  // If the task is waiting for a URL to be generated, register a callback.
  if (ui_service_->IsTaskWaitingForUrl(uuid)) {
    ui_service_->AddPendingUrlCallback(uuid, std::move(wrapped_callback));
    return;
  }

  // There's a slight difference in the callback signature between the mojo
  // api (wants a reference) and the ui service (provided a moved object).
  // The latter can't provide a reference since we're not keeping it
  // long-term, hence wrapping this in a base::BindOnce.
  ui_service_->GetThreadUrlFromTaskId(
      uuid,
      base::BindOnce(
          [](GetUrlForTaskCallback callback, GURL webui_url, GURL url) {
            std::move(callback).Run(contextual_tasks::ContextualTasksUiService::
                                        GetAiUrlFromWebUIUrl(url, webui_url));
          },
          std::move(wrapped_callback), web_ui_controller_->GetWebUiUrl()));
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
  std::move(callback).Run(ContextualTasksUI::IsZeroState(url, ui_service_));
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

void ContextualTasksPageHandler::IsEmbeddedPageErrorDocument(
    IsEmbeddedPageErrorDocumentCallback callback) {
  bool is_error = false;
  if (auto* inner_contents = web_ui_controller_->GetInnerWebContents()) {
    if (auto* main_frame = inner_contents->GetPrimaryMainFrame()) {
      is_error = main_frame->IsErrorDocument();
    }
  }
  std::move(callback).Run(is_error);
}

void ContextualTasksPageHandler::CloseSidePanel() {
  if (panel_controller_) {
    panel_controller_->Close();
  } else {
    web_ui_controller_->CloseSidePanel();
  }
}

void ContextualTasksPageHandler::ShowThreadHistory() {
  // Send a message to AIM to open the threads view.
  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();
  PostMessageToWebview(message);
}

void ContextualTasksPageHandler::IsShownInTab(IsShownInTabCallback callback) {
  if (contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled()) {
    OnPrefChanged();
  }
  std::move(callback).Run(web_ui_controller_->IsShownInTab());
}

void ContextualTasksPageHandler::OpenMyActivityUi() {
  BrowserWindowInterface* browser = web_ui_controller_->GetBrowser();
  if (!browser) {
    return;
  }
  OpenUrlWithDisposition(web_ui_controller_->GetProfile(), GURL(kMyActivityUrl),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB, browser);
}

void ContextualTasksPageHandler::OpenFeedbackUi() {
  if (skip_feedback_ui_for_testing_) {
    return;
  }
  BrowserWindowInterface* browser = web_ui_controller_->GetBrowser();
  if (!browser) {
    return;
  }
  GURL page_url =
      web_ui_controller_->GetWebUIWebContents()->GetLastCommittedURL();
  if (auto* tab_list = TabListInterface::From(browser)) {
    if (auto* active_tab = tab_list->GetActiveTab()) {
      page_url = active_tab->GetContents()->GetLastCommittedURL();
    }
  }

  ui_service_->OpenFeedbackUi(browser, page_url);
}

void ContextualTasksPageHandler::OpenOnboardingHelpUi() {
  BrowserWindowInterface* browser = web_ui_controller_->GetBrowser();
  if (!browser) {
    return;
  }
  OpenUrlWithDisposition(
      web_ui_controller_->GetProfile(),
      GURL(contextual_tasks::GetContextualTasksOnboardingTooltipHelpUrl()),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, browser);
}

void ContextualTasksPageHandler::OpenUrl(const GURL& url,
                                         WindowOpenDisposition disposition) {
  OpenUrlWithDisposition(web_ui_controller_->GetProfile(), url, disposition,
                         web_ui_controller_->GetBrowser());
}

void ContextualTasksPageHandler::MoveTaskUiToNewTab() {
  web_ui_controller_->MoveTaskUiToNewTab();
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

  OMNIBOX_LOG_WITH_PROTO("OnWebviewMessage", aim_to_client_message,
                         std::string("lens.chrome.AimToClientMessage"));

  if (aim_to_client_message.has_handshake_response()) {
    web_ui_controller_->GetPageRemote()->OnHandshakeComplete();
    web_ui_controller_->OnSidePanelStateChanged();
  } else if (aim_to_client_message.has_hide_input()) {
    web_ui_controller_->GetPageRemote()->HideInput();
  } else if (aim_to_client_message.has_restore_input()) {
    web_ui_controller_->GetPageRemote()->RestoreInput();
  } else if (aim_to_client_message.has_enter_basic_mode()) {
    web_ui_controller_->GetPageRemote()->EnterBasicMode();
  } else if (aim_to_client_message
                 .has_set_chrome_desktop_input_plate_configuration()) {
    const auto& update_msg =
        aim_to_client_message.set_chrome_desktop_input_plate_configuration();

    auto mojo_position = contextual_tasks::InputPlateConfigToMojo(update_msg);

    web_ui_controller_->GetPageRemote()->UpdateComposeboxPosition(
        std::move(mojo_position));
  } else if (aim_to_client_message.has_exit_basic_mode()) {
    web_ui_controller_->GetPageRemote()->ExitBasicMode();
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
    OnReceivedInjectInput(aim_to_client_message.inject_input());
  } else if (aim_to_client_message.has_remove_injected_input()) {
    OnReceivedRemoveInjectedInput(
        std::string(aim_to_client_message.remove_injected_input().id()));
  } else if (aim_to_client_message.has_lock_input()) {
    web_ui_controller_->GetPageRemote()->LockInput();
  } else if (aim_to_client_message.has_unlock_input()) {
    web_ui_controller_->GetPageRemote()->UnlockInput();
  } else if (aim_to_client_message.has_open_link_in_side_panel_mode()) {
    tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
        web_ui_controller_->GetWebUIWebContents());
    BrowserWindowInterface* browser = web_ui_controller_->GetBrowser();
    GURL target_url(aim_to_client_message.open_link_in_side_panel_mode().url());
    // Only accept valid URLs that are HTTP or HTTPS.
    if (target_url.is_valid() && target_url.SchemeIsHTTPOrHTTPS()) {
      ui_service_->OnThreadLinkClicked(
          target_url, web_ui_controller_->GetTaskId().value_or(base::Uuid()),
          tab ? tab->GetWeakPtr() : nullptr,
          browser ? browser->GetWeakPtr() : nullptr,
          /*initiator_origin=*/
          web_ui_controller_->GetInnerWebContents()
              ? web_ui_controller_->GetInnerWebContents()
                    ->GetPrimaryMainFrame()
                    ->GetLastCommittedOrigin()
              : url::Origin());
    }
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
  PrefService* prefs = web_ui_controller_->GetProfile()->GetPrefs();
  int count = prefs->GetInteger(
      contextual_tasks::kContextualTasksOnboardingTooltipDismissedCount);
  prefs->SetInteger(
      contextual_tasks::kContextualTasksOnboardingTooltipDismissedCount,
      count + 1);
}

void ContextualTasksPageHandler::ReopenTabs() {
  // TODO(crbug.com/489832161): Implement tab restoration logic.
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

  OMNIBOX_LOG_WITH_PROTO("PostMessageToWebview", message,
                         std::string("lens.chrome.ClientToAimMessage"));

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
      task_id, {},
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

  // Populate restored tabs in the composebox.
  if (contextual_tasks_service_ &&
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    contextual_tasks_service_->GetContextForTask(
        *task_id, {},
        std::make_unique<contextual_tasks::ContextDecorationParams>(),
        base::BindOnce(
            [](base::WeakPtr<ContextualTasksPageHandler> self,
               std::unique_ptr<contextual_tasks::ContextualTaskContext>
                   context) {
              if (self && self->web_ui_controller_) {
                std::vector<contextual_tasks::mojom::ContextInfoPtr>
                    context_items = PopulateContextualResources(context.get());

                std::vector<searchbox::mojom::TabInfoPtr> tabs;
                for (const auto& item : context_items) {
                  if (item->is_tab()) {
                    auto tab_info = searchbox::mojom::TabInfo::New();
                    tab_info->url = item->get_tab()->url;
                    tab_info->title = item->get_tab()->title;
                    tabs.push_back(std::move(tab_info));
                  }
                }
                self->web_ui_controller_->OnRestoredTabsFetched(
                    std::move(tabs));
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ContextualTasksPageHandler::OnReceivedInjectInput(
    const lens::InjectInput& inject_input) {
  contextual_tasks::mojom::InjectedInputPtr mojo_input =
      contextual_tasks::mojom::InjectedInput::New();
  mojo_input->submit_after_injection = inject_input.submit_after_injection();
  mojo_input->query_text = inject_input.query_text();

  if (inject_input.has_modality()) {
    // If the message contains a modality chip, process the chip metadata and
    // register it with the ContextualSearchSessionHandle.
    auto modality =
        std::make_unique<lens::ModalityChipProps>(inject_input.modality());
    contextual_search::ContextualSearchSessionHandle* handle =
        web_ui_controller_->GetOrCreateContextualSessionHandle();
    if (!handle) {
      return;
    }

    // If a chip with the same ID is already injected, clean it up to perform
    // a full override.
    if (modality->has_id()) {
      auto existing_token =
          FindActiveInjectedInputToken(handle, modality->id());
      if (existing_token.has_value()) {
        // Synchronously delete the file from the backend before notifying the
        // UI. This prevents redundant removal notifications back to the server
        // when the UI later asynchronously invokes the `DeleteContext`
        // callback, as that lookup will return nullptr.
        handle->DeleteFile(existing_token.value());
        web_ui_controller_->GetPageRemote()->RemoveInjectedInput(
            existing_token.value());
      }
    }

    auto token = handle->CreateContextToken();

    mojo_input->title = std::string(modality->title());
    mojo_input->file_token = token;
    mojo_input->supports_unimodal = modality->is_unimodal();

    // Notify the front-end UI via Mojo to render the modality chip in the UI
    // carousel, using an icon if provided or otherwise a thumbnail image.
    if (modality->has_icon_id()) {
      mojo_input->icon_id = IconTypeToMojom(modality->icon_id());
    } else {
      mojo_input->thumbnail = std::string(modality->thumbnail_src());
    }

    // Start the chip upload flow. This registers the metadata without executing
    // an actual network upload since the chip data is supplied by the server.
    handle->StartModalityChipUploadFlow(token, std::move(modality));
  }

  web_ui_controller_->GetPageRemote()->InjectInput(std::move(mojo_input));
}

void ContextualTasksPageHandler::OnReceivedRemoveInjectedInput(
    const std::string& id) {
  contextual_search::ContextualSearchSessionHandle* handle =
      web_ui_controller_->GetOrCreateContextualSessionHandle();
  auto token = FindActiveInjectedInputToken(handle, id);
  if (token.has_value()) {
    // Synchronously delete the file from the backend before notifying the UI.
    // This prevents redundant removal notifications back to the server when the
    // UI later asynchronously invokes the `DeleteContext` callback, as that
    // lookup will return nullptr.
    handle->DeleteFile(token.value());
    web_ui_controller_->GetPageRemote()->RemoveInjectedInput(token.value());
  }
}

void ContextualTasksPageHandler::PinSidePanel() {
  if (!contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled()) {
    return;
  }
  web_ui_controller_->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPinContextualTaskButton, true);
}

void ContextualTasksPageHandler::OnContextMenuOpened() {
#if !BUILDFLAG(IS_ANDROID)
  if (!contextual_tasks::ContextualTasksContextService::
          GetIsSmartTabSharingEnabled(web_ui_controller_
                                          ? web_ui_controller_->GetProfile()
                                          : nullptr)) {
    return;
  }
  if (GetSmartTabSharingFeatureActivationCount(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              web_ui_controller_->GetProfile())) > 0) {
    return;
  }
  if (auto* interface =
          BrowserUserEducationInterface::From(webui::GetBrowserWindowInterface(
              web_ui_controller_->GetWebUIWebContents()))) {
    interface->MaybeShowFeaturePromo(
        feature_engagement::kIPHSmartTabSharingFeature);
  }
#endif
}

void ContextualTasksPageHandler::NotifySmartTabSharingTryItIphResult(
    bool accepted) {
#if !BUILDFLAG(IS_ANDROID)
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_ui_controller_->GetProfile());
  if (tracker) {
    if (accepted) {
      if (auto* browser = web_ui_controller_->GetBrowser()) {
        ui_service_->TurnOnSmartTabSharing(browser);
      }

      tracker->NotifyUsedEvent(
          feature_engagement::kIPHSmartTabSharingTryItFeature);
    }
  }
#endif
}

void ContextualTasksPageHandler::NotifySmartTabSharingDefaultOnIphResult(
    bool accepted) {
#if !BUILDFLAG(IS_ANDROID)
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_ui_controller_->GetProfile());
  if (tracker) {
    if (accepted) {
      web_ui_controller_->GetProfile()->GetPrefs()->SetBoolean(
          contextual_tasks::kContextualTasksShareOpenTabsEveryThread, true);

      tracker->NotifyUsedEvent(
          feature_engagement::kIPHSmartTabSharingDefaultOnFeature);
    }
  }
#endif
}

void ContextualTasksPageHandler::RegisterWindow(
    const contextual_tasks::ContextualTaskId& task_id,
    const GURL& url,
    const contextual_tasks::ContextualWindowId& window_id) {
  if (ui_service_) {
    ui_service_->RegisterWindow(task_id, url, window_id);
  }
}

void ContextualTasksPageHandler::CloseWindow(
    const contextual_tasks::ContextualWindowId& window_id) {
  if (ui_service_) {
    ui_service_->CloseTrackedWindow(window_id);
  }
}

void ContextualTasksPageHandler::UnpinSidePanel() {
  if (!contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled()) {
    return;
  }
  web_ui_controller_->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPinContextualTaskButton, false);
}

void ContextualTasksPageHandler::OnPinStateChanged(bool is_pinned) {
  web_ui_controller_->GetPageRemote()->OnSidePanelPinStateChanged(is_pinned);
}

void ContextualTasksPageHandler::OnPrefChanged() {
  OnPinStateChanged(
      contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled() &&
      web_ui_controller_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kPinContextualTaskButton));
}
