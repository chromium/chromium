// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include <optional>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/logger.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_media_link_handler.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using sessions::SessionTabHelper;

namespace contextual_tasks {

namespace {

constexpr net::BackoffEntry::Policy
    kIgnoreFirstErrorRequestAccessTokenBackoffPolicy = {
        // Number of initial errors (in sequence) to ignore before applying
        // exponential back-off rules.
        1,

        // Initial delay for exponential back-off in ms.
        500,

        // Factor by which the waiting time will be multiplied.
        2,

        // Fuzzing percentage. ex: 10% will spread requests randomly
        // between 90%-100% of the calculated time.
        0.2,  // 20%

        // Maximum amount of time we are willing to delay our request in ms.
        10000,  // 10 seconds.

        // Time to keep an entry from being discarded even when it
        // has no significant state, -1 to never discard.
        -1,

        // Don't use initial delay unless the last request was an error.
        false,
};

constexpr char kAiPageHost[] = "https://google.com";
constexpr char kDebugParam[] = "deb";
constexpr char kDebugNoCobrowseValue[] = "nocobrowse1";
constexpr char kNcbParam[] = "ncb";
constexpr char kNcbValue[] = "1";

// Parameters that the search results page must contain at least one of to be
// considered a valid search results page.
constexpr char kSearchQueryKey[] = "q";
constexpr char kLensModeKey[] = "lns_mode";

bool IsSignInDomain(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  std::vector<std::string> sign_in_domains = GetContextualTasksSignInDomains();
  for (const auto& sign_in_domain : sign_in_domains) {
    if (url.host() == sign_in_domain &&
        !base::EndsWith(url.path(), "Logout",
                        base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

// LINT.IfChange(EntrypointSource)

enum class EntrypointSource {
  kFromWeb = 0,
  kFromOmnibox = 1,
  kFromNewTabPage = 2,
  kFromLens = 3,

  kMaxValue = kFromLens,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:EntrypointSource)

EntrypointSource ConvertContextualSearchSourceToEntrypointSource(
    contextual_search::ContextualSearchSource source) {
  switch (source) {
    case contextual_search::ContextualSearchSource::kOmnibox:
      return EntrypointSource::kFromOmnibox;
    case contextual_search::ContextualSearchSource::kNewTabPage:
      return EntrypointSource::kFromNewTabPage;
    case contextual_search::ContextualSearchSource::kLens:
      return EntrypointSource::kFromLens;
    case contextual_search::ContextualSearchSource::kContextualTasks:
      // This shouldn't happen but is it does - just fall through to say it's
      // from web.
    case contextual_search::ContextualSearchSource::kUnknown:
      return EntrypointSource::kFromWeb;
  }
}

}  // namespace

ContextualTasksUiService::ContextualTasksUiService(
    Profile* profile,
    std::unique_ptr<ContextualTasksUiServiceDelegate> delegate,
    ContextualTasksService* contextual_tasks_service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer)
    : profile_(profile),
      delegate_(std::move(delegate)),
      contextual_tasks_service_(contextual_tasks_service),
      identity_manager_(identity_manager),
      aim_eligibility_service_(aim_eligibility_service),
      request_access_token_backoff_(
          &kIgnoreFirstErrorRequestAccessTokenBackoffPolicy),
      cookie_synchronizer_(std::move(cookie_synchronizer)) {
  if (contextual_tasks::ShouldEnableCookiePrefetch() &&
      aim_eligibility_service_) {
    is_cobrowse_eligible_ = aim_eligibility_service_->IsCobrowseEligible();
    aim_eligibility_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(
                &ContextualTasksUiService::OnAimEligibilityChanged,
                base::Unretained(this)));
    if (is_cobrowse_eligible_) {
      EnsureCookiesSynced();
    }
  }
}

ContextualTasksUiService::~ContextualTasksUiService() = default;

void ContextualTasksUiService::EnsureCookiesSynced() {
  if (cookie_synchronizer_) {
    cookie_synchronizer_->CopyCookiesToWebviewStoragePartition();
  }
}

void ContextualTasksUiService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnContextualTasksUiServiceShutdown(this);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  access_token_fetcher_.reset();
  token_refresh_timer_.Stop();
}

void ContextualTasksUiService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksUiService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTasksUiService::OnNavigationToAiPageIntercepted(
    const GURL& url,
    base::WeakPtr<tabs::TabInterface> source_tab,
    bool is_to_new_tab) {
  OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
                              "OnNavigationToAiPageIntercepted called for URL: "
                           << url;
  CHECK(contextual_tasks_service_);

  // Get the session handle from the source web contents, if provided, to
  // propagate context from the source WebUI.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle;
  contextual_search::ContextualSearchSource source =
      contextual_search::ContextualSearchSource::kUnknown;
  if (source_tab) {
    auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
        source_tab->GetContents());
    if (helper && helper->session_handle()) {
      session_handle = helper->TakeSessionHandle();
      source = session_handle->GetMetricsRecorder()->source();
    }
  }
  base::UmaHistogramEnumeration(
      "ContextualTasks.AiPage.NavigationInterceptionSource",
      ConvertContextualSearchSourceToEntrypointSource(source));

  // Create a task for the URL that was just intercepted.
  ContextualTask task = contextual_tasks_service_->CreateTaskFromUrl(url);

  // Map the task ID to the intercepted url. This is done so the UI knows which
  // URL to load initially in the embedded frame.
  task_id_to_creation_url_[task.GetTaskId()] = url;

  GURL ui_url = GetContextualTaskUrlForTask(task.GetTaskId());
  // If the CS param is in the URL, add it to the webui, so the
  // chrome_content_browser_client.cc code can properly setup the renderer dark
  // mode preference. This prevents UI flicker.
  std::optional<bool> is_dark_mode = contextual_tasks::GetDarkModeFromUrl(url);
  if (is_dark_mode.has_value()) {
    ui_url = net::AppendQueryParameter(ui_url, "cs",
                                       is_dark_mode.value() ? "1" : "0");
  }

  content::WebContents* contextual_task_web_contents = nullptr;
  // If the current tab is included in the context list, this navigation should
  // open in the side panel.
  // TODO(crbug.com/462773224): Add test that navigation with current tab in
  // context leads to side panel.
  if (session_handle && source_tab &&
      session_handle->IsTabInContext(
          SessionTabHelper::IdForTab(source_tab->GetContents()))) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "OnNavigationToAiPageIntercepted opening in side panel";
    AssociateWebContentsToTask(source_tab->GetContents(), task.GetTaskId());
    BrowserWindowInterface* window =
        webui::GetBrowserWindowInterface(source_tab->GetContents());
    if (window) {
      auto* controller = ContextualTasksPanelController::From(window);
      controller->Show();
      contextual_task_web_contents = controller->GetActiveWebContents();
    }
  } else if (!is_to_new_tab) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "OnNavigationToAiPageIntercepted loading in source tab";
    source_tab->GetContents()->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(ui_url));
    contextual_task_web_contents = source_tab->GetContents();
  } else {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "OnNavigationToAiPageIntercepted opening in new tab";
    NavigateParams params(profile_, ui_url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

    Navigate(&params);
    contextual_task_web_contents = params.navigated_or_inserted_contents;
  }
  // Associate the web contents with the task and set the session handle if
  // provided.
  if (contextual_task_web_contents) {
    AssociateWebContentsToTask(contextual_task_web_contents, task.GetTaskId());
    if (session_handle) {
      auto* helper =
          ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
              contextual_task_web_contents);
      helper->SetTaskSession(task.GetTaskId(), std::move(session_handle),
                             helper->TakeInputStateModel());
    }
  }
}

void ContextualTasksUiService::OnOAuthTokenReceived(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnOAuthTokenReceived called "
         "with error state: "
      << static_cast<int>(error.state());
  // Clear the fetcher as it's done.
  access_token_fetcher_.reset();

  base::UmaHistogramEnumeration("ContextualTasks.WebUI.OAuthError",
                                error.state(),
                                GoogleServiceAuthError::NUM_STATES);

  if (error.state() != GoogleServiceAuthError::NONE) {
    // If this is a transient error, retry with exponential backoff.
    if (error.IsTransientError()) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: OnOAuthTokenReceived "
             "transient error, will retry";
      request_access_token_backoff_.InformOfRequest(false);
      base::TimeDelta delay =
          request_access_token_backoff_.GetTimeUntilRelease();
      if (delay.is_zero()) {
        OMNIBOX_LOG("nav_trace")
            << "ContextualTasks navigation trace: OnOAuthTokenReceived "
               "transient error, will retry immediately";
        StartAccessTokenFetch();
      } else {
        OMNIBOX_LOG("nav_trace")
            << "ContextualTasks navigation trace: OnOAuthTokenReceived "
               "transient error, will retry in "
            << base::NumberToString(delay.InSeconds()) << " seconds";
        token_refresh_timer_.Start(
            FROM_HERE, delay,
            base::BindOnce(&ContextualTasksUiService::StartAccessTokenFetch,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      return;
    }

    // TODO(crbug.com/470109970): If at this point the token is empty, the error
    // is not transient and a blocking error needs to shown to the user to
    // prevent the user continuing to interact with broken UI.
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: OnOAuthTokenReceived "
           "non-transient error, running callbacks with empty token";
    RunPendingAccessTokenCallbacks("");
    return;
  }
  request_access_token_backoff_.Reset();
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnOAuthTokenReceived "
         "success, running callbacks";
  RunPendingAccessTokenCallbacks(access_token_info.token);
}

void ContextualTasksUiService::ShowOauthErrorDialogForWebContents(
    base::WeakPtr<content::WebContents> web_contents) {
  content::WebUI* webui = web_contents->GetWebUI();
  if (webui && webui->GetController() && webui->GetController()->GetType()) {
    auto* ui_controller = webui->GetController()->GetAs<ContextualTasksUI>();
    if (ui_controller) {
      ui_controller->ShowOauthErrorDialog();
    }
  }
}

void ContextualTasksUiService::RunPendingAccessTokenCallbacks(
    const std::string& token) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: "
         "RunPendingAccessTokenCallbacks called with token empty="
      << token.empty()
      << ", callbacks count=" << pending_access_token_callbacks_.size();
  std::vector<
      std::pair<GetAccessTokenCallback, base::WeakPtr<content::WebContents>>>
      callbacks;
  std::swap(callbacks, pending_access_token_callbacks_);

  if (token.empty()) {
    for (const auto& callback_pair : callbacks) {
      if (callback_pair.second) {
        OMNIBOX_LOG("nav_trace")
            << "ContextualTasks navigation trace: "
               "RunPendingAccessTokenCallbacks showing oauth error dialog";
        ShowOauthErrorDialogForWebContents(callback_pair.second);
      }
    }
  }

  for (auto& callback_pair : callbacks) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "RunPendingAccessTokenCallbacks running callback";
    std::move(callback_pair.first).Run(token);
  }
}

void ContextualTasksUiService::OnAimEligibilityChanged() {
  bool is_cobrowse_eligible = aim_eligibility_service_->IsCobrowseEligible();
  // Trigger cookie sync only on a transition from false to true.
  if (is_cobrowse_eligible && !is_cobrowse_eligible_) {
    EnsureCookiesSynced();
  }
  is_cobrowse_eligible_ = is_cobrowse_eligible;
}

tabs::TabInterface* ContextualTasksUiService::MaybeFocusExistingOpenTab(
    const GURL& url,
    TabListInterface* tab_list,
    const base::Uuid& task_id) {
  GURL url_no_fragments =
      shared_highlighting::RemoveFragmentSelectorDirectives(url);
  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    content::WebContents* web_contents = tab_list->GetTab(i)->GetContents();
    std::optional<ContextualTask> task =
        contextual_tasks_service_->GetContextualTaskForTab(
            SessionTabHelper::IdForTab(web_contents));
    // Remove any text selection directives when trying to match an existing
    // URL. The directives don't meaningfully change the page content, so it's
    // ok to match them.
    GURL tab_url_no_fragments =
        shared_highlighting::RemoveFragmentSelectorDirectives(
            web_contents->GetLastCommittedURL());
    if (tab_url_no_fragments == url_no_fragments && task &&
        task->GetTaskId() == task_id) {
      tab_list->ActivateTab(tab_list->GetTab(i)->GetHandle());
      return tab_list->GetTab(i);
    }
  }
  return nullptr;
}

bool ContextualTasksUiService::MaybeHandleVideoCitation(
    const GURL& url,
    tabs::TabInterface* tab,
    const base::Uuid& task_id) {
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kContextualTasksVideoCitations)) {
    return false;
  }

  if (tab) {
    content::WebContents* web_contents = tab->GetContents();
    std::optional<ContextualTask> task =
        contextual_tasks_service_->GetContextualTaskForTab(
            SessionTabHelper::IdForTab(web_contents));
    if (task && task->GetTaskId() == task_id) {
      auto handler = CreateMediaLinkHandler(web_contents);
      if (handler && handler->MaybeReplaceNavigation(url)) {
        return true;
      }
    }
  }
#endif
  return false;
}

bool ContextualTasksUiService::MaybeHandlePdfCitation(
    const GURL& url,
    tabs::TabInterface* tab,
    const base::Uuid& task_id) {
#if BUILDFLAG(ENABLE_PDF)
  if (!GetIsContextualTasksPdfCitationsEnabled()) {
    return false;
  }

  if (!tab) {
    return false;
  }

  content::WebContents* web_contents = tab->GetContents();
  std::optional<ContextualTask> task =
      contextual_tasks_service_->GetContextualTaskForTab(
          SessionTabHelper::IdForTab(web_contents));

  if (!task || task->GetTaskId() != task_id) {
    return false;
  }

  const GURL& page_url = web_contents->GetLastCommittedURL();
  // The citation URL must match the current page URL (ignoring the fragment).
  if (page_url.GetScheme() != url.GetScheme() ||
      page_url.GetHost() != url.GetHost() ||
      page_url.GetPath() != url.GetPath() ||
      page_url.GetQuery() != url.GetQuery()) {
    return false;
  }

  // The citation URL must have a fragment. The fragment is not parsed here
  // because the PDF viewer supports various open parameters (page, zoom, view,
  // etc.).
  if (url.GetRef().empty()) {
    return false;
  }

  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
  if (!pdf_helper) {
    return false;
  }

  // Dispatch an event to the PDF viewer to update the viewport.
  pdf_extension_util::DispatchShouldUpdateViewportEvent(
      web_contents->GetPrimaryMainFrame(), url);
  return true;
#else
  return false;
#endif
}

void ContextualTasksUiService::OnThreadLinkClicked(
    const GURL& url,
    base::Uuid task_id,
    base::WeakPtr<tabs::TabInterface> tab,
    base::WeakPtr<BrowserWindowInterface> browser) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnThreadLinkClicked called "
         "for URL: "
      << url;
  if (!browser) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: OnThreadLinkClicked "
           "returning early, no browser";
    return;
  }

  TabListInterface* tab_list = TabListInterface::From(browser.get());

  std::string ai_response_link_clicked_metric_name =
      base::StrCat({"ContextualTasks.AiResponse.UserAction.LinkClicked.",
                    (tab ? "Tab" : "Panel")});
  base::UmaHistogramBoolean(ai_response_link_clicked_metric_name, true);
  base::RecordAction(
      base::UserMetricsAction(ai_response_link_clicked_metric_name.c_str()));

  // If the thread link click corresponds to a citation, it should be handled
  // before an unneeded web contents is created below.
  if (!tab) {
    tabs::TabInterface* active_tab = tab_list->GetActiveTab();
    if (MaybeHandleVideoCitation(url, active_tab, task_id) ||
        MaybeHandlePdfCitation(url, active_tab, task_id)) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: OnThreadLinkClicked "
             "citation handled on active tab";
      if (auto* controller =
              ContextualTasksPanelController::From(browser.get())) {
        controller->OnAiInteraction();
      }
      return;
    }
  }

  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_));
  content::WebContents* new_contents_ptr = new_contents.get();
  CreateSessionServiceTabHelper(new_contents_ptr);

  // Copy navigation entries from the current tab to the new tab to support back
  // button navigation. See crbug.com/467042329 for detail.
  if (tab) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: OnThreadLinkClicked "
           "copying navigation entries from tab";
    new_contents->GetController().CopyStateFrom(
        &tab->GetContents()->GetController(), /*needs_reload=*/false);
  }

  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnThreadLinkClicked "
         "loading URL: "
      << url;
  new_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));

  // If the source contents is the panel, open the AI page in a new foreground
  // tab.
  // TODO(crbug.com/458139141): Split this API so we can assume `tab` non-null.
  if (!tab) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: OnThreadLinkClicked source "
           "is panel, opening in tab";
    // Attempt to focus an existing tab prior to creating a new one.
    tabs::TabInterface* existing_tab = nullptr;
    existing_tab = MaybeFocusExistingOpenTab(url, tab_list, task_id);
    if (!existing_tab) {
      if (task_id.is_valid()) {
        AssociateWebContentsToTask(new_contents_ptr, task_id);
      }

      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: OnThreadLinkClicked "
             "using InsertWebContentsAt";
      // Insert the WebContents after the current active.
      int active_tab_index = tab_list->GetActiveIndex();
      tabs::TabInterface* active_tab = tab_list->GetActiveTab();
      tabs::TabInterface* new_tab = tab_list->InsertWebContentsAt(
          active_tab_index + 1, std::move(new_contents),
          /*should_pin=*/false,
          /*group=*/active_tab ? active_tab->GetGroup() : std::nullopt);
      if (active_tab) {
        tab_list->SetOpenerForTab(new_tab->GetHandle(),
                                  active_tab->GetHandle());
      }
      tab_list->ActivateTab(new_tab->GetHandle());
    } else {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: OnThreadLinkClicked "
             "existing tab found";
      // If the tab was found, check if there was a text fragment to search for
      // in the URL. If so, highlight them to be shown to the user.
      std::vector<std::string> fragments =
          shared_highlighting::ExtractTextFragments(url.GetRef());

      content::Page& page = existing_tab->GetContents()->GetPrimaryPage();
      companion::TextFinderManager* text_finder_manager =
          companion::TextFinderManager::GetOrCreateForPage(page);
      text_finder_manager->CreateTextFinders(
          fragments,
          base::BindOnce(&ContextualTasksUiService::OnTextFinderLookupComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         existing_tab->GetWeakPtr(), url, task_id, browser));
    }

    if (auto* controller =
            ContextualTasksPanelController::From(browser.get())) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: OnThreadLinkClicked "
             "counting as part of a cobrowsing session";
      // Count as part of a cobrowsing session if the user interacted with the
      // AI response.
      controller->OnAiInteraction();
    }

    return;
  }

  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnThreadLinkClicked source "
         "is tab, opening linked page in tab directly after";

  // Get the index of the web contents.
  const int current_index = tab_list->GetIndexOfTab(tab->GetHandle());

  // Open the linked page in a tab directly after this one.
  // To prevent side panel to close and reopen again, add the new tab, associate
  // with task and then activate it.
  tabs::TabInterface* new_tab =
      tab_list->InsertWebContentsAt(current_index + 1, std::move(new_contents),
                                    /*should_pin=*/false, tab->GetGroup());

  AssociateWebContentsToTask(new_contents_ptr, task_id);

  DCHECK(new_tab);
  tab_list->ActivateTab(new_tab->GetHandle());
  CHECK(new_contents_ptr == tab_list->GetActiveTab()->GetContents());

  // Detach the WebContents from tab.
  std::unique_ptr<content::WebContents> contextual_task_contents =
      tab_list->DetachWebContents(tab->GetHandle());
  content::WebContents* contextual_task_contents_ptr =
      contextual_task_contents.get();

  // Transfer the contextual task contents into the side panel cache.
  ContextualTasksPanelController::From(browser.get())
      ->TransferWebContentsFromTab(task_id,
                                   std::move(contextual_task_contents));

  // Open the side panel.
  ContextualTasksPanelController::From(browser.get())
      ->Show(/*transition_from_tab=*/true);

  // Notify the WebUI to adjust itself e.g. hide the toolbar.
  // `contextual_task_contents_ptr` is guaranteed to be alive here, since
  // the ownership of `contextual_task_contents` has been moved to
  // the ContextualTasksPanelController implementation's instance.
  if (auto* web_ui_interface =
          GetWebUiInterface(contextual_task_contents_ptr)) {
    web_ui_interface->OnSidePanelStateChanged();
  }
}

void ContextualTasksUiService::OnTextFinderLookupComplete(
    base::WeakPtr<tabs::TabInterface> tab,
    const GURL& url,
    base::Uuid task_id,
    base::WeakPtr<BrowserWindowInterface> browser,
    const std::vector<std::pair<std::string, bool>>& lookup_results) {
  if (!browser) {
    return;
  }

  bool all_text_found = true;
  std::vector<std::string> text_directives;
  for (const auto& pair : lookup_results) {
    if (!pair.second) {
      all_text_found = false;
      break;
    }
    text_directives.push_back(pair.first);
  }

  if (!tab || !all_text_found) {
    // If the tab went away or the text wasn't found on the page, open a new
    // tab.
    std::unique_ptr<content::WebContents> new_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile_));
    content::WebContents* new_contents_ptr = new_contents.get();
    CreateSessionServiceTabHelper(new_contents_ptr);

    new_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));

    AssociateWebContentsToTask(new_contents_ptr, task_id);

    TabListInterface* tab_list = TabListInterface::From(browser.get());
    // Insert the WebContents after the current active.
    int active_tab_index = tab_list->GetActiveIndex();
    tabs::TabInterface* active_tab = tab_list->GetActiveTab();
    tabs::TabInterface* new_tab = tab_list->InsertWebContentsAt(
        active_tab_index + 1, std::move(new_contents),
        /*should_pin=*/false,
        /*group=*/active_tab ? active_tab->GetGroup() : std::nullopt);
    if (active_tab) {
      tab_list->SetOpenerForTab(new_tab->GetHandle(), active_tab->GetHandle());
    }
    tab_list->ActivateTab(new_tab->GetHandle());
    return;
  }

  // Delete any existing `TextHighlighterManager` on the page. Without this, any
  // text highlights after the first to be rendered on the page will not render.
  auto& page = tab->GetContents()->GetPrimaryPage();
  if (companion::TextHighlighterManager::GetForPage(page)) {
    companion::TextHighlighterManager::DeleteForPage(page);
  }

  // If every text fragment was found, then create a text highlighter manager to
  // render the text highlights. Focus the main tab first.
  tab->GetContents()->Focus();
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetOrCreateForPage(page);
  text_highlighter_manager->CreateTextHighlightersAndRemoveExisting(
      text_directives);
}

void ContextualTasksUiService::InitializeTaskInSidePanel(
    content::WebContents* web_contents,
    const base::Uuid& task_id,
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  AssociateWebContentsToTask(web_contents, task_id);
  if (session_handle) {
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents)
        ->SetTaskSession(task_id, std::move(session_handle),
                         /*input_state_model=*/nullptr);
  }
}

void ContextualTasksUiService::OnNonThreadNavigationInTab(
    const GURL& url,
    base::WeakPtr<tabs::TabInterface> tab) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnNonThreadNavigationInTab "
         "called for URL: "
      << url;
  if (!tab || !tab->GetContents()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: OnNonThreadNavigationInTab "
           "returning early, no tab";
    return;
  }

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  tab->GetContents()->GetController().LoadURLWithParams(params);
}

void ContextualTasksUiService::OnSearchResultsNavigationInSidePanel(
    content::OpenURLParams url_params,
    ContextualTasksUIInterface* web_ui_interface) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: "
         "OnSearchResultsNavigationInSidePanel called for URL: "
      << url_params.url;
  web_ui_interface->TransferNavigationToEmbeddedPage(url_params);
}

bool ContextualTasksUiService::HandleNavigation(
    content::OpenURLParams url_params,
    content::WebContents* source_contents,
    bool is_from_embedded_page,
    bool is_to_new_tab,
    bool is_same_site_or_from_ui) {
  return HandleNavigationImpl(
      std::move(url_params), source_contents,
      tabs::TabInterface::MaybeGetFromContents(source_contents),
      is_from_embedded_page, is_to_new_tab, is_same_site_or_from_ui);
}

void ContextualTasksUiService::GetAccessToken(
    GetAccessTokenCallback callback,
    base::WeakPtr<content::WebContents> web_contents) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: GetAccessToken called";
  pending_access_token_callbacks_.emplace_back(std::move(callback),
                                               web_contents);

  // If a request is already in progress, or we are waiting to retry, do
  // nothing.
  if (access_token_fetcher_ || token_refresh_timer_.IsRunning()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: GetAccessToken returning "
           "early because fetch is in progress or waiting to retry";
    return;
  }

  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: GetAccessToken starting "
         "access token fetch";
  StartAccessTokenFetch();
}

void ContextualTasksUiService::StartAccessTokenFetch() {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: StartAccessTokenFetch called";
  token_refresh_timer_.Stop();

  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: StartAccessTokenFetch "
           "returning early due to no primary account";
    RunPendingAccessTokenCallbacks("");
    return;
  }

  auto account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: StartAccessTokenFetch "
         "creating access token fetcher";
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account.account_id, signin::OAuthConsumerId::kContextualTasks,
      base::BindOnce(&ContextualTasksUiService::OnOAuthTokenReceived,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void ContextualTasksUiService::OnShareUrlNavigation(const GURL& url) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: OnShareUrlNavigation called "
         "for URL: "
      << url;
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

bool ContextualTasksUiService::HandleNavigationImpl(
    content::OpenURLParams url_params,
    content::WebContents* source_contents,
    tabs::TabInterface* tab,
    bool is_from_embedded_page,
    bool is_to_new_tab,
    bool is_same_site_or_from_ui) {
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: HandleNavigationImpl called "
         "for URL: "
      << url_params.url;
  // Make sure the user is eligible to use the feature before attempting to
  // intercept.
  if (!contextual_tasks_service_ ||
      !contextual_tasks_service_->GetFeatureEligibility().IsEligible()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "returning early, not eligible";
    return false;
  }

  // If the target URL is a contextual tasks "display URL", then replace it with
  // the proper AIM URL.
  bool original_url_is_virtual = false;
  if (IsContextualTasksDisplayUrl(url_params.url)) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "navigating to contextual tasks display URL";
    original_url_is_virtual = true;
    const GURL aim_url =
        GetUrlForAim(TemplateURLServiceFactory::GetForProfile(profile_.get()),
                     omnibox::UNKNOWN_AIM_ENTRY_POINT, base::Time::Now(), u"",
                     std::nullopt, {});

    GURL::Replacements replacements;
    replacements.SetSchemeStr(aim_url.scheme());
    replacements.SetHostStr(aim_url.host());
    replacements.SetPathStr(aim_url.path());

    url_params.url = url_params.url.ReplaceComponents(replacements);
  }
  if (IsContextualTasksUrl(url_params.url)) {
    if (is_from_embedded_page) {
      DCHECK(false) << "Unexpected URL from embedded page " << url_params.url;
      return true;
    }

#if !BUILDFLAG(IS_ANDROID)
    // If the navigation is a back/forward navigation, the resulting URL is a
    // contextual tasks URL, and the panel is open, this indicates that the user
    // is navigating back to the full tab contextual tasks page. Instead of
    // allowing the navigation, move contextual tasks from the panel to a full
    // tab and close the current tab.
    bool is_forward_back_navigation =
        url_params.transition & ui::PAGE_TRANSITION_FORWARD_BACK;
    if (base::FeatureList::IsEnabled(
            kContextualTasksBackButtonExpandsSidePanel) &&
        !url_params.is_renderer_initiated && is_forward_back_navigation &&
        tab) {
      auto* controller = contextual_tasks::ContextualTasksPanelController::From(
          tab->GetBrowserWindowInterface());
      if (controller->IsPanelOpenForContextualTask()) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::WeakPtr<tabs::TabInterface> weak_tab) {
                  if (!weak_tab) {
                    return;
                  }
                  auto* browser = weak_tab->GetBrowserWindowInterface();
                  if (!browser) {
                    return;
                  }
                  auto* controller =
                      contextual_tasks::ContextualTasksPanelController::From(
                          browser);
                  if (controller &&
                      controller->IsPanelOpenForContextualTask()) {
                    controller->MoveTaskUiToNewTab();
                    // TODO(crbug.com/497899043): Restore history stack.
                    auto* tab_strip_model = browser->GetTabStripModel();
                    if (tab_strip_model) {
                      int index =
                          tab_strip_model->GetIndexOfTab(weak_tab.get());
                      if (index != TabStripModel::kNoTab) {
                        tab_strip_model->CloseWebContentsAt(
                            index, TabCloseTypes::CLOSE_EXPAND_SIDE_PANEL);
                      }
                    }
                  }
                },
                tab->GetWeakPtr()));
        OMNIBOX_LOG("nav_trace")
            << "ContextualTasks navigation trace: HandleNavigationImpl "
               "closing tab and expanding side panel";
        return true;
      }
    }
#endif

    // Else, allow any navigation to the contextual tasks host.
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "returning early, navigating to contextual tasks host";
    return false;
  }

  bool is_nav_to_ai = IsAiUrl(url_params.url);

  // The "deb=nocobrowse1" and "ncb=1" params allow bypassing interception.
  bool should_bypass_interception = false;
  std::string bypass_reason;
  std::string ncb_value;
  std::string debug_param_value;

  if (is_nav_to_ai) {
    if (net::GetValueForKeyInQuery(url_params.url, kNcbParam, &ncb_value) &&
        ncb_value == kNcbValue) {
      should_bypass_interception = true;
      bypass_reason = "ncb param";
    } else if (net::GetValueForKeyInQuery(url_params.url, kDebugParam,
                                        &debug_param_value) &&
               debug_param_value.contains(kDebugNoCobrowseValue)) {
      should_bypass_interception = true;
      bypass_reason = "debug param";
    }

    // If the page is to AI and the navigation is not same site, apply a param
    // to the URL to mark it as untrusted. Likewise, remove it if present and
    // the navigation is same site.
    if (!is_same_site_or_from_ui) {
      url_params.url =
          net::AppendOrReplaceQueryParameter(url_params.url, "cru", "1");
    } else {
      url_params.url = net::AppendOrReplaceQueryParameter(url_params.url, "cru",
                                                          std::nullopt);
    }
  }

  if (should_bypass_interception) {
    if (original_url_is_virtual) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: HandleNavigationImpl "
             "posting LoadUrlInWebContents for "
          << bypass_reason;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ContextualTasksUiService::LoadUrlInWebContents,
                         weak_ptr_factory_.GetWeakPtr(), url_params.url,
                         source_contents));
      return true;
    } else {
      return false;
    }
  }

  // If the user is not signed in to Chrome, do not intercept.
  if (!IsSignedInToBrowserWithValidCredentials()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "returning false, not signed into browser";
    return false;
  }

  // If the user is not signed in to the account that is using the URL, do not
  // intercept.
  if (is_nav_to_ai && !IsUrlForPrimaryAccount(url_params.url)) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "returning false, not signed into account for AI URL";
    return false;
  }

  // At this point, the user is signed in to Chrome and signed into the account
  // that is using the URL. From here on out, the navigation can be intercepted.
  bool is_nav_to_sign_in = IsSignInDomain(url_params.url);

  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface()
          : webui::GetBrowserWindowInterface(source_contents);

  // Whether the navigation is from forward back navigation and originally from
  // a link click. `page_transition` can contain both the original navigation
  // information (from link click or typed, etc) and the modified one(from
  // forward/back).
  ui::PageTransition page_transition = url_params.transition;
  bool is_forward_back_link_navigation =
      (page_transition & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      ui::PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_LINK);

  // Intercept any navigation where the wrapping WebContents is the WebUI host
  // unless it is the embedded page.
  if ((is_from_embedded_page || is_forward_back_link_navigation) &&
      IsContextualTasksUrl(source_contents->GetLastCommittedURL())) {
    if (IsShareUrl(url_params.url)) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: HandleNavigationImpl "
             "posting OnShareUrlNavigation";
      // Since the web content will no longer be hosted in the side panel, make
      // sure to remove the param that makes the page render for it.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ContextualTasksUiService::OnShareUrlNavigation,
                         weak_ptr_factory_.GetWeakPtr(),
                         lens::RemoveSidePanelURLParameters(url_params.url)));
      return true;
    }

    // Ignore navigation triggered by UI except forward back link navigation.
    if (!(url_params.is_renderer_initiated ||
          is_forward_back_link_navigation)) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: HandleNavigationImpl "
             "returning false, not renderer initiated";
      return false;
    }
    // Allow users to sign in within the <webview>.
    // TODO(crbug.com/454388385): Remove this once the authentication flow is
    // implemented.
    if (is_nav_to_sign_in) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: HandleNavigationImpl "
             "returning false, sign in domain";
      return false;
    }

    base::Uuid task_id;
    if (source_contents) {
      task_id = GetTaskIdFromUrl(source_contents->GetLastCommittedURL());
    }

    // If the navigation is to a search results page or AI page, it is allowed
    // if being viewed in the side panel, but only if it is intercepted without
    // the side panel-specific params. If the params have already been added, do
    // nothing, otherwise this logic causes an infinite "intercept" loop. Any
    // "allowed domain" (e.g. Google) should not be treated as a thread link
    // unless the link is expected to open in a new tab (e.g. the anchor tag was
    // set to target="_blank").
    bool is_allowed_host = IsAllowedHost(url_params.url) &&
                           !(is_from_embedded_page && is_to_new_tab);
    if (is_allowed_host || is_nav_to_ai) {
      if (tab) {
        if (!is_nav_to_ai) {
          OMNIBOX_LOG("nav_trace")
              << "ContextualTasks navigation trace: HandleNavigationImpl "
                 "posting OnNonThreadNavigationInTab";
          // The SRP should never be embedded in the WebUI when viewed in a tab.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &ContextualTasksUiService::OnNonThreadNavigationInTab,
                  weak_ptr_factory_.GetWeakPtr(), url_params.url,
                  tab->GetWeakPtr()));
          return true;
        } else {
          OMNIBOX_LOG("nav_trace")
              << "ContextualTasks navigation trace: HandleNavigationImpl "
                 "returning false, embedded AI page";
          // Allow any navigations to an AI page from embedded page.
          return false;
        }
      } else if (IsValidSearchResultsPage(url_params.url) || is_nav_to_ai) {
        if (!lens::HasCommonSearchQueryParameters(url_params.url)) {
          OMNIBOX_LOG("nav_trace")
              << "ContextualTasks navigation trace: HandleNavigationImpl "
                 "posting OnSearchResultsNavigationInSidePanel";
          ContextualTasksUIInterface* webui_controller =
              GetWebUiInterface(source_contents);

          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&ContextualTasksUiService::
                                 OnSearchResultsNavigationInSidePanel,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(url_params), webui_controller));
          return true;
        }

        // If the params are present and the page is "valid" (e.g. not
        // shopping and has a query), allow the navigation.
        OMNIBOX_LOG("nav_trace")
            << "ContextualTasks navigation trace: HandleNavigationImpl "
               "returning false, valid SRP with params";
        return false;
      }
    }

    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "posting OnThreadLinkClicked";
    // This needs to be posted in case the called method triggers a navigation
    // in the same WebContents, invalidating the nav handle used up the chain.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksUiService::OnThreadLinkClicked,
                       weak_ptr_factory_.GetWeakPtr(), url_params.url, task_id,
                       tab ? tab->GetWeakPtr() : nullptr,
                       browser ? browser->GetWeakPtr() : nullptr));
    return true;
  }

  // Navigations to the AI URL in the topmost frame should always be
  // intercepted.
  if (is_nav_to_ai) {
    if (!aim_eligibility_service_->IsCobrowseEligible()) {
      OMNIBOX_LOG("nav_trace")
          << "ContextualTasks navigation trace: HandleNavigationImpl "
             "returning false, nav to AI but not cobrowse eligible";
      return false;
    }

    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: HandleNavigationImpl "
           "posting OnNavigationToAiPageIntercepted";
    // This needs to be posted in case the called method triggers a navigation
    // in the same WebContents, invalidating the nav handle used up the chain.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ContextualTasksUiService::OnNavigationToAiPageIntercepted,
            weak_ptr_factory_.GetWeakPtr(), url_params.url,
            tab ? tab->GetWeakPtr() : nullptr, is_to_new_tab));
    return true;
  }

  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: HandleNavigationImpl "
         "returning false at end of method";
  // Allow anything else.
  return false;
}

void ContextualTasksUiService::LoadUrlInWebContents(
    const GURL& url,
    content::WebContents* web_contents) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  web_contents->GetController().LoadURLWithParams(params);
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<lens::LensMediaLinkHandler>
ContextualTasksUiService::CreateMediaLinkHandler(
    content::WebContents* web_contents) {
  return std::make_unique<lens::LensMediaLinkHandler>(web_contents);
}
#endif

bool ContextualTasksUiService::IsUrlForPrimaryAccount(const GURL& url) {
  return contextual_tasks::IsUrlForPrimaryAccount(identity_manager_, url);
}

bool ContextualTasksUiService::IsSignedInToBrowserWithValidCredentials() {
  if (!identity_manager_) {
    return false;
  }

  // If the primary account doesn't have a refresh token, the <webview> will
  // not be properly authenticated, so treat this as signed out.
  if (!identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    return false;
  }

  // Verify that the primary account refresh token does not have any errors. If
  // it does, the <webview> will not be properly authenticated, so treat as
  // signed out.
  const CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  return !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
      primary_account);
}

bool ContextualTasksUiService::CookieJarContainsPrimaryAccount() {
  return contextual_tasks::CookieJarContainsPrimaryAccount(identity_manager_);
}

omnibox::ChromeAimEntryPoint
ContextualTasksUiService::GetInitialEntryPointForTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_entry_point_override_.find(task_id);
  if (it != task_id_to_entry_point_override_.end()) {
    return it->second;
  }
  return omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT;
}
void ContextualTasksUiService::OpenFeedbackUi(BrowserWindowInterface* browser,
                                              const GURL& page_url) {
  if (delegate_) {
    delegate_->OpenFeedbackUi(browser, page_url);
  }
}

void ContextualTasksUiService::ShowUndoSnackbar(
    BrowserWindowInterface* browser_window_interface) {
  if (delegate_) {
    delegate_->ShowUndoSnackbar(browser_window_interface);
  }
}

GURL ContextualTasksUiService::GetContextualTaskUrlForTask(
    const base::Uuid& task_id) {
  GURL url(chrome::kChromeUIContextualTasksURL);

  std::string host = GetHostForTask(task_id);
  if (!host.empty()) {
    url = net::AppendQueryParameter(url, kChromeHostParam, host);
  }

  url = net::AppendQueryParameter(url, kTaskQueryParam,
                                  task_id.AsLowercaseString());

  omnibox::ChromeAimEntryPoint entry_point =
      GetInitialEntryPointForTask(task_id);
  return AppendAimEntryPointParams(url, entry_point);
}

std::string ContextualTasksUiService::GetHostForTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_creation_url_.find(task_id);
  if (it != task_id_to_creation_url_.end()) {
    std::string host;
    if (net::GetValueForKeyInQuery(it->second, kChromeHostParam, &host)) {
      return host;
    }

    std::string_view creation_host = it->second.host();
    GURL default_ai_url(kAiPageHost);
    std::string_view default_host = default_ai_url.host();

    std::string stripped_host =
        url_formatter::StripWWW(std::string(creation_host));
    std::string stripped_default_host =
        url_formatter::StripWWW(std::string(default_host));

    if (!stripped_host.empty() && stripped_host != stripped_default_host) {
      return std::string(creation_host);
    }
  }

  std::string forced_host = GetForcedEmbeddedPageHost();
  if (!forced_host.empty()) {
    return forced_host;
  }

  return "";
}

bool ContextualTasksUiService::IsTrustedHost(const std::string& host) {
  if (base::EndsWith(host, ".corp.google.com") ||
      base::EndsWith(host, ".c.googlers.com") ||
      base::EndsWith(host, ".proxy.googlers.com")) {
    return true;
  }

  if (host == "localhost" || host == "127.0.0.1" || host == "[::1]") {
    return true;
  }

  return false;
}

void ContextualTasksUiService::SetInitialEntryPointForTask(
    const base::Uuid& task_id,
    omnibox::ChromeAimEntryPoint entry_point) {
  if (entry_point != omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT) {
    task_id_to_entry_point_override_[task_id] = entry_point;
  }
}

std::optional<GURL> ContextualTasksUiService::GetInitialUrlForTask(
    const base::Uuid& uuid) {
  auto it = task_id_to_creation_url_.find(uuid);
  if (it != task_id_to_creation_url_.end()) {
    GURL url = it->second;
    // Ensure the sourceid param is set. This is needed to identify chrome
    // source traffic for AIM pages that were directly navigated to by the user,
    // as opposed to threads created by Chrome.
    url = net::AppendOrReplaceQueryParameter(url, "sourceid", "chrome");
    url = net::AppendOrReplaceQueryParameter(url, "ccb", "1");
    task_id_to_creation_url_.erase(it);
    omnibox::ChromeAimEntryPoint entry_point =
        GetInitialEntryPointForTask(uuid);
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: GetInitialUrlForTask "
           "returning URL with entry point";
    return AppendAimEntryPointParams(url, entry_point);
  }
  OMNIBOX_LOG("nav_trace")
      << "ContextualTasks navigation trace: GetInitialUrlForTask "
         "returning nullopt";
  return std::nullopt;
}

std::optional<GURL> ContextualTasksUiService::GetCreationUrlForTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_creation_url_.find(task_id);
  if (it != task_id_to_creation_url_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void ContextualTasksUiService::AddPendingUrlCallback(
    const base::Uuid& task_id,
    base::OnceCallback<void(const GURL&)> callback) {
  tasks_waiting_for_url_[task_id] = std::move(callback);
}

bool ContextualTasksUiService::IsTaskWaitingForUrl(const base::Uuid& task_id) {
  return tasks_waiting_for_url_.contains(task_id);
}

void ContextualTasksUiService::GetThreadUrlFromTaskId(
    const base::Uuid& task_id,
    base::OnceCallback<void(GURL)> callback) {
  contextual_tasks_service_->GetThreadUrlFromTaskId(
      task_id, g_browser_process->GetApplicationLocale(),
      GetInitialEntryPointForTask(task_id), std::move(callback));
}

GURL ContextualTasksUiService::GetDefaultAiPageUrl() {
  return GURL(GetContextualTasksAiPageUrl());
}

GURL ContextualTasksUiService::GetDefaultAiPageUrlForTask(
    const base::Uuid& task_id) {
  GURL url = GetDefaultAiPageUrl();

  omnibox::ChromeAimEntryPoint entry_point =
      GetInitialEntryPointForTask(task_id);
  return AppendAimEntryPointParams(url, entry_point);
}

void ContextualTasksUiService::OnTaskChanged(
    BrowserWindowInterface* browser_window_interface,
    content::WebContents* web_contents,
    const std::optional<base::Uuid>& old_task_id,
    const std::optional<base::Uuid>& new_task_id,
    bool is_shown_in_tab) {
  if (!browser_window_interface) {
    return;
  }

  if (delegate_) {
    delegate_->OnTaskChanged(browser_window_interface, old_task_id,
                             new_task_id);
  }

  ContextualTasksPanelController* controller =
      ContextualTasksPanelController::From(browser_window_interface);

  if (is_shown_in_tab) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_.get());
    UpdateContextualSearchWebContentsHelperForTask(
        contextual_search_service, browser_window_interface,
        contextual_tasks_service_, controller, web_contents,
        new_task_id.value_or(base::Uuid()));

    auto* active_task_context_provider =
        ActiveTaskContextProvider::From(browser_window_interface);
    if (active_task_context_provider) {
      active_task_context_provider->RefreshContext();
    }
  } else {  // !is_shown_in_tab
    // If a new thread is started in the panel, affiliated tabs should change
    // their thread to the new one.
    base::Uuid final_task_id = new_task_id.value_or(base::Uuid());
    if (!final_task_id.is_valid()) {
      // If the panel is in zero state, create an empty task.
      ContextualTask task = contextual_tasks_service_->CreateTask();
      final_task_id = task.GetTaskId();
    }

    TabListInterface* tab_list =
        TabListInterface::From(browser_window_interface);
    content::WebContents* active_contents =
        tab_list->GetActiveTab()->GetContents();
    SessionID active_id = SessionTabHelper::IdForTab(active_contents);

    // If the current tab is associated with any task, change associations for
    // all tabs associated with that task.
    std::optional<ContextualTask> current_task =
        contextual_tasks_service_->GetContextualTaskForTab(active_id);
    if (current_task) {
      std::vector<SessionID> tab_ids =
          contextual_tasks_service_->GetTabsAssociatedWithTask(
              current_task->GetTaskId());
      for (const auto& id : tab_ids) {
        contextual_tasks_service_->AssociateTabWithTask(final_task_id, id);
      }
    }

    controller->OnTaskChanged(web_contents, final_task_id);
  }
}

void ContextualTasksUiService::OnWebUIReady(
    BrowserWindowInterface* browser_window_interface,
    const base::Uuid& task_id,
    content::WebContents* web_contents) {
  if (delegate_) {
    delegate_->OnWebUIReady(browser_window_interface, task_id, web_contents);
  }
}

void ContextualTasksUiService::OnWebUIDestroyed(
    BrowserWindowInterface* browser_window_interface,
    const std::optional<base::Uuid>& task_id) {
  if (delegate_) {
    delegate_->OnWebUIDestroyed(browser_window_interface, task_id);
  }
}

void ContextualTasksUiService::TurnOnSmartTabSharing(
    BrowserWindowInterface* browser) {
  if (!browser || browser->GetProfile() != profile_) {
    return;
  }

  // Check side panel.
  auto* controller = ContextualTasksPanelController::From(browser);
  if (controller && controller->IsPanelOpenForContextualTask()) {
    content::WebContents* web_contents = controller->GetActiveWebContents();
    if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
      if (web_ui_interface->GetPageRemote().is_bound()) {
        web_ui_interface->GetPageRemote()->TurnOnSmartTabSharing();
      }
    }
  }

  // Check all tabs.
  TabListInterface* tab_list = TabListInterface::From(browser);
  if (tab_list) {
    for (int i = 0; i < tab_list->GetTabCount(); ++i) {
      content::WebContents* web_contents = tab_list->GetTab(i)->GetContents();
      if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
        if (web_ui_interface->GetPageRemote().is_bound()) {
          web_ui_interface->GetPageRemote()->TurnOnSmartTabSharing();
        }
      }
    }
  }
}

void ContextualTasksUiService::MoveTaskUiToNewTab(
    const base::Uuid& task_id,
    BrowserWindowInterface* browser,
    const GURL& inner_frame_url) {
  auto* controller = ContextualTasksPanelController::From(browser);
  CHECK(controller);

  // If the side panel wasn't showing an AI page, don't embed the page in the
  // webui - navigate directly to the link instead.
  if (!IsAiUrl(inner_frame_url)) {
    // Since the web content will no longer be hosted in the side panel, make
    // sure to remove the param that makes the page render for it.
    NavigateParams params(browser,
                          lens::RemoveSidePanelURLParameters(inner_frame_url),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);

  } else {
    std::unique_ptr<content::WebContents> web_contents =
        controller->DetachWebContentsForTask(task_id);
    if (!web_contents) {
      return;
    }

    // Make sure to acquire a raw pointer handle to the WebContents prior to
    // std::moving it below since it's used later in this function.
    auto* web_contents_ptr = web_contents.get();
    NavigateParams params(browser, std::move(web_contents));
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.transition = ui::PAGE_TRANSITION_LINK;
    Navigate(&params);

    // Notify the WebUI that the tab status has changed only after the contents
    // has been moved to a tab.
    if (auto* web_ui_interface = GetWebUiInterface(web_contents_ptr)) {
      web_ui_interface->OnSidePanelStateChanged();
    }
  }

  controller->Close();

  ContextualTasksService* task_service =
      ContextualTasksServiceFactory::GetForProfile(browser->GetProfile());
  CHECK(task_service);
  task_service->DisassociateAllTabsFromTask(task_id);
}

void ContextualTasksUiService::StartTaskUiInSidePanel(
    BrowserWindowInterface* browser_window_interface,
    tabs::TabInterface* tab_interface,
    const GURL& url,
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  CHECK(!url.is_empty());
  CHECK(contextual_tasks_service_);

  // Get the controller for the current window.
  auto* controller =
      ContextualTasksPanelController::From(browser_window_interface);
  auto* panel_contents = controller->GetActiveWebContents();

  // Create a task for the URL if the side panel wasn't already showing a task.
  if (!panel_contents || !controller->IsPanelOpenForContextualTask()) {
    ContextualTask task = contextual_tasks_service_->CreateTaskFromUrl(url);
    task_id_to_creation_url_[task.GetTaskId()] = url;
    AssociateWebContentsToTask(tab_interface->GetContents(), task.GetTaskId());
    controller->Show();

    InitializeTaskInSidePanel(controller->GetActiveWebContents(),
                              task.GetTaskId(), std::move(session_handle));
    return;
  }

  // If the side panel contents already exist, get the WebUI controller to
  // load the URL into the already loaded contextual tasks UI.
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      panel_contents);
  // If the task was waiting for a URL to be generated (e.g. opened early
  // with ghost loader but no URL), provide the URL now to unblock the WebUI's
  // initial pull request via GetUrlForTask.
  if (helper->task_id().has_value() &&
      IsTaskWaitingForUrl(helper->task_id().value())) {
    OnInitialThreadUrlAvailable(helper->task_id().value(), url);
    return;
  }

  // Otherwise, if the panel is already open and initialized, push the
  // navigation directly to the embedded page.
  if (ContextualTasksUIInterface* web_ui_interface =
          GetWebUiInterface(panel_contents)) {
    content::OpenURLParams url_params(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
    web_ui_interface->TransferNavigationToEmbeddedPage(url_params);
  }
}

void ContextualTasksUiService::InitSidePanelWithGhostLoader(
    BrowserWindowInterface* browser_window_interface,
    tabs::TabInterface* tab_interface,
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  CHECK(contextual_tasks_service_);

  // Get the controller for the current window.
  auto* controller =
      ContextualTasksPanelController::From(browser_window_interface);
  auto* panel_contents = controller->GetActiveWebContents();
  // If the side panel is already open for a task, do nothing. The ghost
  // loader will be shown whenever the URL navigation is transferred to the
  // embedded page.
  if (panel_contents || controller->IsPanelOpenForContextualTask()) {
    return;
  }

  // Create a task for the URL if the side panel wasn't already showing a task.
  ContextualTask task = contextual_tasks_service_->CreateTask();
  tasks_waiting_for_url_[task.GetTaskId()] = base::NullCallback();
  AssociateWebContentsToTask(tab_interface->GetContents(), task.GetTaskId());
  controller->Show();

  InitializeTaskInSidePanel(controller->GetActiveWebContents(),
                            task.GetTaskId(), std::move(session_handle));
}

void ContextualTasksUiService::StartTaskUiInSidePanelWithErrorPage(
    BrowserWindowInterface* browser_window_interface,
    tabs::TabInterface* tab_interface,
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  // Create a new task.
  ContextualTask task = contextual_tasks_service_->CreateTask();
  auto* controller =
      ContextualTasksPanelController::From(browser_window_interface);
  auto* panel_contents = controller->GetActiveWebContents();
  auto source = session_handle && session_handle->GetMetricsRecorder()
                    ? session_handle->GetMetricsRecorder()->source()
                    : contextual_search::ContextualSearchSource::kUnknown;

  if (tab_interface) {
    AssociateWebContentsToTask(tab_interface->GetContents(), task.GetTaskId());
  }

  bool panel_was_closed =
      !panel_contents || !controller->IsPanelOpenForContextualTask();
  if (panel_was_closed) {
    pending_error_page_tasks_.emplace(task.GetTaskId(), source);
    controller->Show();
  }

  content::WebContents* web_contents = controller->GetActiveWebContents();
  InitializeTaskInSidePanel(web_contents, task.GetTaskId(),
                            std::move(session_handle));

  if (!panel_was_closed) {
    if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
      ShowAndRecordErrorPage(web_ui_interface->GetPageRemote(), source);
    }
  }
}

bool ContextualTasksUiService::IsAiUrl(const GURL& url) {
  if (!IsSearchResultsUrl(url)) {
    return false;
  }

  return aim_eligibility_service_->HasAimUrlParams(url);
}

bool ContextualTasksUiService::IsPendingErrorPage(const base::Uuid& task_id) {
  if (!pending_error_page_tasks_.contains(task_id)) {
    return false;
  }
  RecordErrorPageShown(pending_error_page_tasks_[task_id]);
  return true;
}

bool ContextualTasksUiService::IsContextualTasksUrl(const GURL& url) {
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}

base::Uuid ContextualTasksUiService::GetTaskIdFromUrl(const GURL& url) {
  std::string task_id;
  net::GetValueForKeyInQuery(url, kTaskQueryParam, &task_id);
  return base::Uuid::ParseLowercase(task_id);
}

bool ContextualTasksUiService::IsContextualTasksDisplayUrl(const GURL& url) {
  return url.scheme() == GetContextualTasksDisplayUrlScheme() &&
         url.host() == GetContextualTasksDisplayUrlHost() &&
         url.path() == GetContextualTasksDisplayUrlPath();
}

bool ContextualTasksUiService::IsSearchResultsUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || !IsAllowedHost(url)) {
    return false;
  }

  if (!base::StartsWith(url.path(), "/search")) {
    return false;
  }

  return true;
}

bool ContextualTasksUiService::IsShareUrl(const GURL& url) {
  return url.query().find("https%3A%2F%2Fshare.google%2Faimode") !=
         std::string::npos;
}

bool ContextualTasksUiService::IsValidSearchResultsPage(const GURL& url) {
  if (!IsSearchResultsUrl(url)) {
    return false;
  }

  // Do not allow shopping mode queries.
  std::string value;
  if (net::GetValueForKeyInQuery(url, "udm", &value) && value == "28") {
    return false;
  }

  // The search results page is only valid if it has a text query or is a Lens
  // query.
  return (net::GetValueForKeyInQuery(url, kSearchQueryKey, &value) &&
          !value.empty()) ||
         (net::GetValueForKeyInQuery(url, kLensModeKey, &value) &&
          !value.empty());
}

GURL ContextualTasksUiService::CopyParamsFromWebUIUrl(const GURL& base_url,
                                                      const GURL& webui_url) {
  std::string host_value;
  GURL aim_url(base_url);

  // Extract host if present in WebUI URL and prepend it to make it
  // first.
  if (net::GetValueForKeyInQuery(webui_url, kChromeHostParam, &host_value)) {
    if (IsTrustedHost(host_value)) {
      GURL::Replacements replacements;
      std::string new_query = base::StrCat({kChromeHostParam, "=", host_value});
      replacements.SetQueryStr(new_query);
      aim_url = base_url.ReplaceComponents(replacements);

      // The QueryIterator correctly iterates over duplicate keys, and
      // GetUnescapedValue preserves their values. This ensures that duplicate
      // parameters on base_url are not lost during the transfer.
      net::QueryIterator base_it(base_url);
      while (!base_it.IsAtEnd()) {
        std::string key(base_it.GetKey());
        if (key != kChromeHostParam) {
          aim_url = net::AppendQueryParameter(aim_url, key,
                                              base_it.GetUnescapedValue());
        }
        base_it.Advance();
      }
    }
  }

  // Now add all other params from the WebUI URL.
  net::QueryIterator it(webui_url);
  while (!it.IsAtEnd()) {
    std::string key(it.GetKey());
    if (key != kTaskQueryParam && key != kChromeHostParam) {
      std::string value;
      net::GetValueForKeyInQuery(webui_url, key, &value);
      aim_url = net::AppendOrReplaceQueryParameter(aim_url, key, value);
    }
    it.Advance();
  }

  return aim_url;
}

void ContextualTasksUiService::OnLensOverlayStateChanged(
    BrowserWindowInterface* browser_window_interface,
    bool is_showing,
    std::optional<lens::LensOverlayInvocationSource> invocation_source) {
  auto* controller =
      ContextualTasksPanelController::From(browser_window_interface);
  if (!controller || !controller->IsPanelOpenForContextualTask()) {
    return;
  }

  auto* panel_contents = controller->GetActiveWebContents();
  if (!panel_contents) {
    return;
  }

  auto* web_ui_interface = GetWebUiInterface(panel_contents);
  if (web_ui_interface) {
    web_ui_interface->OnLensOverlayStateChanged(is_showing, invocation_source);
  }
}

void ContextualTasksUiService::AssociateWebContentsToTask(
    content::WebContents* web_contents,
    const base::Uuid& task_id) {
  SessionID session_id = SessionTabHelper::IdForTab(web_contents);
  if (session_id.is_valid()) {
    contextual_tasks_service_->AssociateTabWithTask(task_id, session_id);
  }
}

void ContextualTasksUiService::OnTabClickedFromSourcesMenu(
    int32_t tab_id,
    const GURL& url,
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    // TODO(crbug.com/460614856): Handle PDF and other possible contexts.
    return;
  }

  // Find the tab on the tab strip by the given tab ID. If found, switch to it.
  // Chances are that the tab might have navigated by now, hence check the URL
  // as well.
  TabListInterface* tab_list = TabListInterface::From(browser);
  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    content::WebContents* web_contents = tab_list->GetTab(i)->GetContents();
    tabs::TabInterface* tab_interface =
        tabs::TabInterface::GetFromContents(web_contents);
    if (tab_interface && tab_interface->GetHandle().raw_value() == tab_id &&
        web_contents->GetLastCommittedURL() == url) {
      tab_list->ActivateTab(tab_interface->GetHandle());
      return;
    }
  }

  // The tab with the given ID and URL wasn't found. Next, try finding a tab
  // that matches the URL. If found, switch to it.
  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    tabs::TabInterface* tab_interface = tab_list->GetTab(i);
    if (tab_interface->GetContents()->GetLastCommittedURL() == url) {
      tab_list->ActivateTab(tab_interface->GetHandle());
      return;
    }
  }

  // The tab wasn't found. Open a new tab with the given URL to the end of the
  // tab strip.
  NavigateParams params(browser->GetProfile(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ContextualTasksUiService::OnFileClickedFromSourcesMenu(
    const GURL& url,
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  NavigateParams params(browser->GetProfile(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ContextualTasksUiService::OnImageClickedFromSourcesMenu(
    const GURL& url,
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  NavigateParams params(browser->GetProfile(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

bool ContextualTasksUiService::IsAllowedHost(const GURL& url) {
  // TODO(crbug.com/498566984): Remove this once the AimEligibilityService tells
  //                            us which hosts to intercept.
  bool is_lens_debug_host = url.host() == "lndb.corp.google.com" ||
                            url.host() == "lndb-autopush.corp.google.com";
  if (net::SchemefulSite::IsSameSite(url, GURL(kAiPageHost)) &&
      !is_lens_debug_host) {
    // Exclude lens debugging hosts.
    return true;
  }
  std::string forced_host = GetForcedEmbeddedPageHost();
  if (!forced_host.empty() &&
      net::SchemefulSite::IsSameSite(
          url,
          GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                             forced_host})))) {
    return true;
  }
  return false;
}

void ContextualTasksUiService::OnInitialThreadUrlAvailable(
    const base::Uuid& task_id,
    const GURL& url) {
  task_id_to_creation_url_[task_id] = url;
  auto it = tasks_waiting_for_url_.find(task_id);
  if (it != tasks_waiting_for_url_.end()) {
    if (it->second) {
      std::move(it->second).Run(GetInitialUrlForTask(task_id).value());
    }
    tasks_waiting_for_url_.erase(it);
  }
}

}  // namespace contextual_tasks
