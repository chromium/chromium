// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#endif
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/android/contextual_tasks_panel_host_android.h"
#include "chrome/browser/contextual_tasks/android/contextual_tasks_panel_host_desktop_android.h"
#else
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host_desktop.h"
#endif

namespace contextual_tasks {

bool IsContextualTasksUrl(const GURL& url) {
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}

std::unique_ptr<
    contextual_search::ContextualSearchContextController::ConfigParams>
CreateQueryControllerConfigParams() {
  auto config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  config_params->send_lns_surface = true;
  config_params->enable_viewport_images = true;
  config_params->attach_page_title_and_url_to_suggest_requests = false;
  return config_params;
}

void ShowAndRecordErrorPage(mojo::Remote<contextual_tasks::mojom::Page>& page,
                            contextual_search::ContextualSearchSource source) {
  if (page) {
    page->ShowErrorPage();
  }
  RecordErrorPageShown(source);
}

void RecordErrorPageShown(contextual_search::ContextualSearchSource source) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.ErrorPageShown", ".",
                    contextual_search::ContextualSearchMetricsRecorder::
                        ContextualSearchSourceToString(source)}),
      contextual_search::ContextualSearchErrorPage::kPageContextNotEligible);
}

void RecordInnerFrameContentsHttpResponseCode(int http_status_code,
                                              bool is_zero_state) {
  base::UmaHistogramSparse(
      "ContextualTasks.InnerFrameContents.HttpResponseCode", http_status_code);
  if (!is_zero_state) {
    base::UmaHistogramSparse(
        "ContextualTasks.InnerFrameContents.HttpResponseCode."
        "ExcludeZeroStateLoads",
        http_status_code);
  }
}

ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  content::WebUI* web_ui = web_contents->GetWebUI();
  if (!web_ui) {
    return nullptr;
  }

  content::WebUIController* controller = web_ui->GetController();
  if (!controller || !controller->GetType()) {
    return nullptr;
  }

  return controller->GetAs<ContextualTasksUI>();
}

bool IsValidUrlForSuggestedTab(const GURL& url,
                               Profile* profile,
                               SiteExclusionDetail& site_exclusion_detail) {
  if (!url.is_valid() || url.IsAboutBlank()) {
    return false;
  }

  if (!(url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile())) {
    return false;
  }

  if (search::IsNTPOrRelatedURL(url, profile)) {
    return false;
  }

  CHECK(profile);
  base::ElapsedTimer timer;
  // Since site exclusions are expected to be rare, it is generally faster
  // and simpler to use list-like key processing instead of allocating with
  // `url.GetHost()` and then having to check the dictionary for various
  // domain substrings. Using `DomainIs` means sites like `en.wikipedia.org`
  // will be filtered if the site exclusions contain `wikipedia.org`.
  site_exclusion_detail.tabs_checked++;
  for (auto it : ReadSiteExclusionsFromPrefs(profile->GetPrefs())) {
    site_exclusion_detail.exclusions_checked++;
    if (url.DomainIs(it.first)) {
      site_exclusion_detail.tabs_filtered++;
      site_exclusion_detail.duration += timer.Elapsed();
      return false;
    }
  }
  site_exclusion_detail.duration += timer.Elapsed();

  return true;
}

std::unique_ptr<contextual_search::ContextualSearchContextController::
                    CreateClientToAimRequestInfo>
PrepareClientToAimRequestInfo(
    const std::string& query,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    ContextualTasksUIInterface* web_ui_interface,
    omnibox::ToolMode active_tool,
    omnibox::ModelMode active_model,
    std::optional<int64_t> active_tab_context_id,
    std::optional<base::UnguessableToken> overlay_token,
    bool is_voice_search) {
  CHECK(web_ui_interface);
  auto info =
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>();
  info->query_text = query;
  info->query_text_source =
      is_voice_search ? lens::QueryPayload::QUERY_TEXT_SOURCE_VOICE_INPUT
                      : lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT;
  info->query_start_time = base::Time::Now();
  if (overlay_token) {
    info->overlay_token = overlay_token;
  }

  info->active_tool = active_tool;
  info->active_model = active_model;

  if (active_tab_context_id.has_value()) {
    lens::ContextTurnMetadata active_tab_context_turn_metadata;
    active_tab_context_turn_metadata.set_context_id(*active_tab_context_id);
    active_tab_context_turn_metadata.mutable_tab_metadata()->set_is_active_tab(
        true);
    info->context_turn_metadata.push_back(active_tab_context_turn_metadata);
  }

  base::flat_set<base::UnguessableToken> file_tokens;
  if (session_handle) {
    file_tokens = session_handle->GetUploadedContextTokens();
  }

  for (const auto& token : file_tokens) {
    const contextual_search::FileInfo* file_info =
        session_handle->GetController()->GetFileInfo(token);
    if (file_info && file_info->GetInjectedInputId().has_value()) {
      SendInjectedInputRemovedUpdate(web_ui_interface,
                                     file_info->GetInjectedInputId().value());
    }
  }

  if (overlay_token.has_value()) {
    file_tokens.insert(*overlay_token);
    // When an overlay token is present, it implies a recent Lens Overlay
    // interaction, such as a region search. Setting this flag forces the
    // inclusion of that interaction's data in the request. This is required
    // to support immediate postmessage-based follow-up queries after the
    // initial search URL loads, allowing the user to ask follow-up questions
    // about the same region without re-selecting it.
    info->force_include_latest_interaction_request_data = true;
  }

  info->file_tokens = std::move(file_tokens).extract();

  return info;
}

void FinalizeAndSendAimQuery(
    std::unique_ptr<contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> request_info,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    ContextualTasksUIInterface* web_ui_interface) {
  if (!session_handle || !web_ui_interface) {
    return;
  }

  lens::ClientToAimMessage client_to_page_message =
      session_handle->CreateClientToAimRequest(std::move(request_info));

  web_ui_interface->PostMessageToWebview(client_to_page_message);
}

void SendInjectedInputRemovedUpdate(
    ContextualTasksUIInterface* web_ui_interface,
    const std::string& id) {
  CHECK(web_ui_interface);

  lens::ClientToAimMessage client_to_aim_message;
  lens::InjectedInputUpdate* injected_input_update =
      client_to_aim_message.mutable_injected_input_update();
  injected_input_update->mutable_payload()->set_id(id);
  injected_input_update->mutable_payload()->set_update_type(
      lens::InjectedInputUpdatePayload::UpdateType::
          InjectedInputUpdatePayload_UpdateType_REMOVED);

  web_ui_interface->PostMessageToWebview(client_to_aim_message);
}

bool ShouldShowSidePanel() {
#if BUILDFLAG(IS_ANDROID)
  bool is_tablet_or_desktop =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET ||
       ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP);
  return is_tablet_or_desktop &&
         !base::FeatureList::IsEnabled(
             kContextualTasksOverrideShowBottomSheetOnLargeScreen);
#else
  return true;
#endif
}

GURL GetContextualTasksFunctionalURL(content::WebContents* web_contents) {
  auto* ui = GetWebUiInterface(web_contents);
  return ui ? ui->GetInnerFrameUrl() : GURL();
}

GURL GetContextualTasksDisplayURL(content::WebContents* web_contents) {
  GURL inner_frame_url = GetContextualTasksFunctionalURL(web_contents);
  return location_bar_model::GetContextualTasksDisplayURL(inner_frame_url);
}

// static
std::unique_ptr<ContextualTasksPanelHost> ContextualTasksPanelHost::Create(
    BrowserWindowInterface* browser_window) {
#if BUILDFLAG(IS_ANDROID)
  if (ShouldShowSidePanel()) {
    return std::make_unique<ContextualTasksPanelHostDesktopAndroid>(
        browser_window);
  } else {
    return std::make_unique<ContextualTasksPanelHostAndroid>(browser_window);
  }
#else
  return std::make_unique<ContextualTasksPanelHostDesktop>(browser_window);
#endif
}

bool GetEffectivePinState(Profile* profile) {
  if (!profile) {
    return false;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (auto* model = PinnedToolbarActionsModel::Get(profile)) {
    return model->Contains(kActionSidePanelShowContextualTasks);
  }
#endif
  return false;
}

}  // namespace contextual_tasks
