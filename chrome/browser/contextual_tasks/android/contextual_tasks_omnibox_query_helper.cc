// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_omnibox_query_helper.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_url_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/page_transition_types.h"

namespace contextual_tasks {

namespace {

using CreateSearchUrlRequestInfo = contextual_search::
    ContextualSearchContextController::CreateSearchUrlRequestInfo;

// Desktop values are reused until dedicated Android values are added to the
// proto; see ContextualTasksBridge::OpenAskGoogleAboutThisPage().
constexpr omnibox::ChromeAimEntryPoint kEntryPoint =
    omnibox::DESKTOP_CHROME_COBROWSE_OMNIBOX_CONTEXTUAL_SUGGESTION;
constexpr lens::LensOverlayInvocationSource kInvocationSource =
    lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion;

}  // namespace

// static
bool ContextualTasksOmniboxQueryHelper::IsEligible(
    content::WebContents* web_contents) {
  if (!kContextualTasksEntryPointsOmniboxContextualSuggestions.Get() ||
      !ContextualTasksBridge::ShouldShowAskGoogleEntryPoints(web_contents)) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return contextual_search::ContextualSearchService::IsContextSharingEnabled(
      profile->GetPrefs());
}

// static
void ContextualTasksOmniboxQueryHelper::Start(
    content::WebContents* web_contents,
    const GURL& suggestion_url) {
  if (!web_contents || !suggestion_url.is_valid()) {
    return;
  }
  if (IsEligible(web_contents)) {
    GetOrCreateForWebContents(web_contents)->StartQuery(suggestion_url);
    return;
  }
  // Avoid attaching an observer to an ineligible `WebContents`; cancel any
  // in-flight query and fall back to a plain navigation.
  if (auto* helper = FromWebContents(web_contents)) {
    helper->Reset();
  }
  LoadInTab(web_contents, suggestion_url);
}

ContextualTasksOmniboxQueryHelper::ContextualTasksOmniboxQueryHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ContextualTasksOmniboxQueryHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

ContextualTasksOmniboxQueryHelper::~ContextualTasksOmniboxQueryHelper() =
    default;

void ContextualTasksOmniboxQueryHelper::PrimaryPageChanged(
    content::Page& page) {
  Reset();
}

void ContextualTasksOmniboxQueryHelper::StartQuery(const GURL& suggestion_url) {
  Reset();
  suggestion_url_ = suggestion_url;
  if (!TryStartContextualQuery()) {
    LoadInTabAndReset(suggestion_url);
  }
}

bool ContextualTasksOmniboxQueryHelper::TryStartContextualQuery() {
  content::WebContents* web_contents = &GetWebContents();
  if (!GetSidePanelUiService()) {
    return false;
  }
  auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  auto* tab_controller =
      tab ? lens::TabContextualizationController::From(tab) : nullptr;
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* search_service = ContextualSearchServiceFactory::GetForProfile(profile);
  if (!tab_controller || !search_service) {
    return false;
  }

  auto config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  config_params->send_lns_surface = false;
  config_params->enable_viewport_images = true;
  session_handle_ = search_service->CreateSession(
      std::move(config_params),
      contextual_search::ContextualSearchSource::kOmnibox, kInvocationSource);
  if (!session_handle_ || !session_handle_->CheckSearchContentSharingSettings(
                              profile->GetPrefs())) {
    session_handle_.reset();
    return false;
  }
  session_handle_->NotifySessionStarted();
  tab_ = tab->GetWeakPtr();

  page_context_request_id_ = base::UnguessableToken::Create();
  tab_controller->GetPageContext(
      base::BindOnce(&ContextualTasksOmniboxQueryHelper::OnTabContextCaptured,
                     weak_ptr_factory_.GetWeakPtr()),
      page_context_request_id_);
  return true;
}

void ContextualTasksOmniboxQueryHelper::OnTabContextCaptured(
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  page_context_request_id_ = base::UnguessableToken();
  if (!tab_ || !session_handle_ || !tab_->IsActivated()) {
    Reset();
    return;
  }
  if (page_content_data &&
      !page_content_data->is_page_context_eligible.value_or(false)) {
    if (GetIsProtectedPageErrorEnabled()) {
      // `GetSidePanelUiService()` was non-null when the query started; if the
      // panel went away since, fall back to a plain search.
      if (auto* ui_service = GetSidePanelUiService()) {
        ui_service->StartTaskUiInSidePanelWithErrorPage(
            tab_->GetBrowserWindowInterface(), tab_.get(),
            std::move(session_handle_), kEntryPoint);
        Reset();
      } else {
        LoadInTabAndReset(suggestion_url_);
      }
      return;
    }
    page_content_data.reset();
  }
  if (page_content_data && page_content_data->context_input.has_value() &&
      !page_content_data->context_input->empty()) {
    page_content_data->is_implicit_upload = true;
    page_content_data->upload_type =
        lens::LensOverlayContextualInputUploadType::
            CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;
    session_handle_->StartTabContextUploadFlow(
        session_handle_->CreateContextToken(), std::move(page_content_data),
        CreateImageEncodingOptions());
  }
  CreateSearchUrl();
}

void ContextualTasksOmniboxQueryHelper::CreateSearchUrl() {
  auto request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  net::GetValueForKeyInQuery(suggestion_url_, "q", &request_info->query_text);
  request_info->additional_params =
      lens::GetParametersMapWithoutQuery(suggestion_url_);
  // `CreateSearchUrl()` populates `aep` from `aim_entry_point`.
  request_info->additional_params.erase("aep");
  request_info->query_start_time = base::Time::Now();
  request_info->search_url_type =
      contextual_search::ContextualSearchContextController::SearchUrlType::kAim;
  request_info->aim_entry_point = kEntryPoint;
  request_info->lens_overlay_selection_type =
      lens::MULTIMODAL_SUGGEST_ZERO_PREFIX;
  request_info->invocation_source = kInvocationSource;

  session_handle_->CreateSearchUrl(
      std::move(request_info),
      base::BindOnce(&ContextualTasksOmniboxQueryHelper::OnSearchUrlCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualTasksOmniboxQueryHelper::OnSearchUrlCreated(GURL url) {
  if (!tab_ || !tab_->IsActivated()) {
    Reset();
    return;
  }
  auto* ui_service = GetSidePanelUiService();
  if (url.is_empty() || !ui_service) {
    LoadInTabAndReset(suggestion_url_);
    return;
  }
  ui_service->StartTaskUiInSidePanel(
      tab_->GetBrowserWindowInterface(), tab_.get(), url,
      std::move(session_handle_),
      StartTaskUiOptions{.entry_point = kEntryPoint});
  Reset();
}

ContextualTasksUiService*
ContextualTasksOmniboxQueryHelper::GetSidePanelUiService() {
  content::WebContents* web_contents = &GetWebContents();
  auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  BrowserWindowInterface* browser_window =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!browser_window ||
      !ContextualTasksPanelController::From(browser_window)) {
    return nullptr;
  }
  return ContextualTasksUiServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
}

// static
void ContextualTasksOmniboxQueryHelper::LoadInTab(
    content::WebContents* web_contents,
    const GURL& url) {
  web_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());
}

void ContextualTasksOmniboxQueryHelper::LoadInTabAndReset(const GURL& url) {
  LoadInTab(&GetWebContents(), url);
  Reset();
}

void ContextualTasksOmniboxQueryHelper::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!page_context_request_id_.is_empty()) {
    auto* tab = tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
    if (auto* tab_controller =
            tab ? lens::TabContextualizationController::From(tab) : nullptr) {
      tab_controller->CancelPageContextRequest(page_context_request_id_);
    }
    page_context_request_id_ = base::UnguessableToken();
  }
  session_handle_.reset();
  tab_ = nullptr;
  suggestion_url_ = GURL();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksOmniboxQueryHelper);

}  // namespace contextual_tasks
