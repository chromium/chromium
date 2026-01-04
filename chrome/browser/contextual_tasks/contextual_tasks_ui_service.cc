// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_url_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

using sessions::SessionTabHelper;

namespace contextual_tasks {

namespace {
constexpr char kAiPageHost[] = "https://google.com";
constexpr char kTaskQueryParam[] = "task";

// Parameters that the search results page must contain at least one of to be
// considered a valid search results page.
constexpr char kSearchQueryKey[] = "q";
constexpr char kLensModeKey[] = "lns_mode";

// Search parameters for the AI page.
// TODO(crbug.com/466149941): These should be more robust to be able to handle
// changes in the URL format.
constexpr char kUdmParam[] = "udm";
constexpr char kUdmAiValue[] = "50";
constexpr char kNemParam[] = "nem";
constexpr char kNemAiValue[] = "143";

// Query parameter values for the mode.
inline constexpr char kShoppingModeParameterValue[] = "28";

bool IsSignInDomain(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  std::vector<std::string> sign_in_domains = GetContextualTasksSignInDomains();
  for (const auto& sign_in_domain : sign_in_domains) {
    if (url.host() == sign_in_domain) {
      return true;
    }
  }
  return false;
}

// Gets the contextual task Id from a contextual task host URL.
base::Uuid GetTaskIdFromHostURL(const GURL& url) {
  std::string task_id;
  net::GetValueForKeyInQuery(url, kTaskQueryParam, &task_id);
  return base::Uuid::ParseLowercase(task_id);
}

// LINT.IfChange(EntrypointSource)

enum class EntrypointSource {
  kFromWeb = 0,
  kFromOmnibox = 1,
  kFromNewTabPage = 2,

  kMaxValue = kFromNewTabPage,
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
    case contextual_search::ContextualSearchSource::kContextualTasks:
      // These shouldn't happen but if they do - just fall through to say it's
      // from web.
    case contextual_search::ContextualSearchSource::kUnknown:
      return EntrypointSource::kFromWeb;
  }
}

}  // namespace

ContextualTasksUiService::ContextualTasksUiService(
    Profile* profile,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    signin::IdentityManager* identity_manager)
    : profile_(profile),
      contextual_tasks_service_(contextual_tasks_service),
      identity_manager_(identity_manager) {
  ai_page_host_ = GURL(kAiPageHost);
}

ContextualTasksUiService::~ContextualTasksUiService() = default;

void ContextualTasksUiService::OnNavigationToAiPageIntercepted(
    const GURL& url,
    base::WeakPtr<tabs::TabInterface> source_tab,
    bool is_to_new_tab) {
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
      source = helper->session_handle()->GetMetricsRecorder()->source();

      auto* service =
          ContextualSearchServiceFactory::GetForProfile(profile_.get());
      if (service) {
        // Create a new handle for existing session. The session handle should
        // not be moved because there can be cases where a user opens the WebUI
        // in a new tab (and would therefore leave the source tab without a
        // handle).
        session_handle =
            service->GetSession(helper->session_handle()->session_id());
        if (session_handle) {
          session_handle->set_submitted_context_tokens(
              helper->session_handle()->GetSubmittedContextTokens());
          // TODO(crbug.com/469877869): Determine what to do with the return
          // value of this call, or move this call to a different location.
          session_handle->CheckSearchContentSharingSettings(
              profile_->GetPrefs());
        }
      }
    }
  }
  base::UmaHistogramEnumeration(
      "ContextualTasks.AiPage.NavigationInterceptionSource",
      ConvertContextualSearchSourceToEntrypointSource(source));

  // Create a task for the URL that was just intercepted.
  ContextualTask task = contextual_tasks_service_->CreateTaskFromUrl(url);

  // Map the task ID to the intercepted url. This is done so the UI knows which
  // URL to load initially in the embedded frame.
  GURL query_url = lens::AppendCommonSearchParametersToURL(
      url, g_browser_process->GetApplicationLocale(), false);
  task_id_to_creation_url_[task.GetTaskId()] = query_url;

  GURL ui_url = GetContextualTaskUrlForTask(task.GetTaskId());

  content::WebContents* contextual_task_web_contents = nullptr;
  // If the current tab is included in the context list, this navigation should
  // open in the side panel.
  // TODO(crbug.com/462773224): Add test that navigation with current tab in
  // context leads to side panel.
  if (session_handle && source_tab &&
      session_handle->IsTabInContext(
          SessionTabHelper::IdForTab(source_tab->GetContents()))) {
    AssociateWebContentsToTask(source_tab->GetContents(), task.GetTaskId());
    BrowserWindow* window = BrowserWindow::FindBrowserWindowWithWebContents(
        source_tab->GetContents());
    if (window) {
      auto* coordinator = ContextualTasksSidePanelCoordinator::From(
          window->AsBrowserView()->browser());
      coordinator->Show();
      contextual_task_web_contents = coordinator->GetActiveWebContents();
    }
  } else if (!is_to_new_tab) {
    source_tab->GetContents()->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(ui_url));
    contextual_task_web_contents = source_tab->GetContents();
  } else {
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
      ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
          contextual_task_web_contents)
          ->SetTaskSession(task.GetTaskId(), std::move(session_handle));
    }
  }
}

bool ContextualTasksUiService::MaybeFocusExistingOpenTab(
    const GURL& url,
    TabStripModel* tab_strip_model,
    const base::Uuid& task_id) {
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* web_contents =
        tab_strip_model->GetTabAtIndex(i)->GetContents();
    std::optional<ContextualTask> task =
        contextual_tasks_service_->GetContextualTaskForTab(
            SessionTabHelper::IdForTab(web_contents));
    if (web_contents->GetLastCommittedURL() == url && task &&
        task->GetTaskId() == task_id) {
      tab_strip_model->ActivateTabAt(i);
      return true;
    }
  }
  return false;
}

void ContextualTasksUiService::OnThreadLinkClicked(
    const GURL& url,
    base::Uuid task_id,
    base::WeakPtr<tabs::TabInterface> tab,
    base::WeakPtr<BrowserWindowInterface> browser) {
  if (!browser) {
    return;
  }

  std::string ai_response_link_clicked_metric_name =
      base::StrCat({"ContextualTasks.AiResponse.UserAction.LinkClicked.",
                    (tab ? "Tab" : "Panel")});
  base::UmaHistogramBoolean(ai_response_link_clicked_metric_name, true);
  base::RecordAction(
      base::UserMetricsAction(ai_response_link_clicked_metric_name.c_str()));

  TabStripModel* tab_strip_model = browser->GetTabStripModel();
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_));
  content::WebContents* new_contents_ptr = new_contents.get();

  // Copy navigation entries from the current tab to the new tab to support back
  // button navigation. See crbug.com/467042329 for detail.
  if (tab && kOpenSidePanelOnLinkClicked.Get()) {
    new_contents->GetController().CopyStateFrom(
        &tab->GetContents()->GetController(), /*needs_reload=*/false);
  }

  new_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));

  // If the source contents is the panel, open the AI page in a new foreground
  // tab.
  // TODO(crbug.com/458139141): Split this API so we can assume `tab` non-null.
  if (!tab) {
    // Attempt to focus an existing tab prior to creating a new one.
    if (!MaybeFocusExistingOpenTab(url, tab_strip_model, task_id)) {
      // Creates the Tab so session ID is created for the WebContents.
      auto tab_to_insert = std::make_unique<tabs::TabModel>(
          std::move(new_contents), tab_strip_model);
      if (task_id.is_valid()) {
        AssociateWebContentsToTask(new_contents_ptr, task_id);
      }
      // Insert the WebContents after the current active.
      int active_tab_index = tab_strip_model->active_index();
      tab_strip_model->AddTab(std::move(tab_to_insert), active_tab_index + 1,
                              ui::PAGE_TRANSITION_LINK,
                              AddTabTypes::ADD_ACTIVE);
    }

    return;
  }

  // Get the index of the web contents.
  const int current_index = tab_strip_model->GetIndexOfTab(tab.get());

  // Open the linked page in a tab directly after this one.
  tab_strip_model->InsertWebContentsAt(
      current_index + 1, std::move(new_contents), AddTabTypes::ADD_ACTIVE);
  if (tab->GetGroup()) {
    tab_strip_model->AddToExistingGroup({current_index + 1},
                                        tab->GetGroup().value());
  }

  CHECK(new_contents_ptr == tab_strip_model->GetActiveWebContents());
  AssociateWebContentsToTask(new_contents_ptr, task_id);

  // Do not open side panel if kOpenSidePanelOnLinkClicked is not set.
  if (!kOpenSidePanelOnLinkClicked.Get()) {
    return;
  }

  // Detach the WebContents from tab.
  std::unique_ptr<content::WebContents> contextual_task_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(
          current_index,
          TabStripModelChange::RemoveReason::kInsertedIntoSidePanel);
  content::WebContents* contextual_task_contents_ptr =
      contextual_task_contents.get();

  // Transfer the contextual task contents into the side panel cache.
  ContextualTasksSidePanelCoordinator::From(browser.get())
      ->TransferWebContentsFromTab(task_id,
                                   std::move(contextual_task_contents));

  // Open the side panel.
  ContextualTasksSidePanelCoordinator::From(browser.get())
      ->Show(/*transition_from_tab=*/true);

  // Notify the WebUI to adjust itself e.g. hide the toolbar.
  // `contextual_task_contents_ptr` is guaranteed to be alive here, since
  // the ownership of `contextual_task_contents` has been moved to
  // ContextualTasksSidePanelCoordinator.
  content::WebUI* webui = contextual_task_contents_ptr->GetWebUI();
  if (webui && webui->GetController()) {
    webui->GetController()
        ->GetAs<ContextualTasksUI>()
        ->OnSidePanelStateChanged();
  }
}

void ContextualTasksUiService::OnSearchResultsNavigationInTab(
    const GURL& url,
    base::WeakPtr<tabs::TabInterface> tab) {
  if (!tab || !tab->GetContents()) {
    return;
  }

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  tab->GetContents()->GetController().LoadURLWithParams(params);
}

void ContextualTasksUiService::OnSearchResultsNavigationInSidePanel(
    content::OpenURLParams url_params,
    ContextualTasksUI* webui_controller) {
  url_params.url = lens::AppendCommonSearchParametersToURL(
      url_params.url, g_browser_process->GetApplicationLocale(), false);
  webui_controller->TransferNavigationToEmbeddedPage(url_params);
}

bool ContextualTasksUiService::HandleNavigation(
    content::OpenURLParams url_params,
    content::WebContents* source_contents,
    bool is_from_embedded_page,
    bool is_to_new_tab) {
  return HandleNavigationImpl(
      std::move(url_params), source_contents,
      tabs::TabInterface::MaybeGetFromContents(source_contents),
      is_from_embedded_page, is_to_new_tab);
}

bool ContextualTasksUiService::HandleNavigationImpl(
    content::OpenURLParams url_params,
    content::WebContents* source_contents,
    tabs::TabInterface* tab,
    bool is_from_embedded_page,
    bool is_to_new_tab) {
  // Make sure the user is eligible to use the feature before attempting to
  // intercept.
  if (!contextual_tasks_service_ ||
      !contextual_tasks_service_->GetFeatureEligibility().IsEligible()) {
    return false;
  }

  // Allow any navigation to the contextual tasks host.
  if (IsContextualTasksUrl(url_params.url)) {
    return false;
  }

  bool is_nav_to_ai = IsAiUrl(url_params.url);

  // If the user is not signed in to Chrome, do not intercept.
  if (!IsSignedInToBrowserWithValidCredentials()) {
    return false;
  }

  // If the user is not signed in to the account that is using the URL, do not
  // intercept.
  if (is_nav_to_ai && !IsUrlForPrimaryAccount(url_params.url)) {
    return false;
  }

  // At this point, the user is signed in to Chrome and signed into the account
  // that is using the URL. From here on out, the navigation can be intercepted.
  bool is_nav_to_sign_in = IsSignInDomain(url_params.url);

  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface()
          : webui::GetBrowserWindowInterface(source_contents);

  // Intercept any navigation where the wrapping WebContents is the WebUI host
  // unless it is the embedded page.
  if (is_from_embedded_page &&
      IsContextualTasksUrl(source_contents->GetLastCommittedURL())) {
    // Ignore navigation triggered by UI.
    if (!url_params.is_renderer_initiated) {
      return false;
    }
    // Allow users to sign in within the <webview>.
    // TODO(crbug.com/454388385): Remove this once the authentication flow is
    // implemented.
    if (is_nav_to_sign_in) {
      return false;
    }

    base::Uuid task_id;
    if (source_contents) {
      task_id = GetTaskIdFromHostURL(source_contents->GetLastCommittedURL());
    }

    // If the navigation is to a search results page or AI page, it is allowed
    // if being viewed in the side panel, but only if it is intercepted without
    // the side panel-specific params. If the params have already been added, do
    // nothing, otherwise this logic causes an infinite "intercept" loop.
    if (IsValidSearchResultsPage(url_params.url) || is_nav_to_ai) {
      // The search results page needs to be handled differently depending on
      // whether viewed in a tab or side panel.
      if (tab && !is_nav_to_ai) {
        // The SRP should never be embedded in the WebUI when viewed in a tab.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ContextualTasksUiService::OnSearchResultsNavigationInTab,
                weak_ptr_factory_.GetWeakPtr(), url_params.url,
                tab->GetWeakPtr()));
        return true;
      } else if (!lens::HasCommonSearchQueryParameters(url_params.url)) {
        // If a navigation to search results happened without the common
        // params and in the side panel, it needs special handling.
        ContextualTasksUI* webui_controller = nullptr;
        if (source_contents->GetWebUI()) {
          webui_controller = source_contents->GetWebUI()
                                 ->GetController()
                                 ->GetAs<ContextualTasksUI>();
        }

        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ContextualTasksUiService::OnSearchResultsNavigationInSidePanel,
                weak_ptr_factory_.GetWeakPtr(), std::move(url_params),
                webui_controller));
        return true;
      } else {
        // If already in the side panel and the custom params are present,
        // allow the navigation.
        return false;
      }
    }

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

  // Allow anything else.
  return false;
}

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

GURL ContextualTasksUiService::GetContextualTaskUrlForTask(
    const base::Uuid& task_id) {
  GURL url(chrome::kChromeUIContextualTasksURL);
  url = net::AppendQueryParameter(url, kTaskQueryParam,
                                  task_id.AsLowercaseString());
  return url;
}

std::optional<GURL> ContextualTasksUiService::GetInitialUrlForTask(
    const base::Uuid& uuid) {
  auto it = task_id_to_creation_url_.find(uuid);
  if (it != task_id_to_creation_url_.end()) {
    GURL url = it->second;
    task_id_to_creation_url_.erase(it);
    return std::move(url);
  }
  return std::nullopt;
}

void ContextualTasksUiService::GetThreadUrlFromTaskId(
    const base::Uuid& task_id,
    base::OnceCallback<void(GURL)> callback) {
  contextual_tasks_service_->GetTaskById(
      task_id, base::BindOnce(
                   [](base::WeakPtr<ContextualTasksUiService> service,
                      base::OnceCallback<void(GURL)> callback,
                      std::optional<ContextualTask> task) {
                     if (!service) {
                       std::move(callback).Run(GURL());
                       return;
                     }

                     GURL url = service->GetDefaultAiPageUrl();
                     if (!task) {
                       std::move(callback).Run(url);
                       return;
                     }

                     std::optional<Thread> thread = task->GetThread();
                     if (!thread) {
                       std::move(callback).Run(url);
                       return;
                     }

                     // Attach the thread ID and the most recent turn ID to the
                     // URL. A query parameter needs to be present, but its
                     // value is not used for continued threads.
                     url = net::AppendQueryParameter(url, "q", thread->title);
                     url = net::AppendQueryParameter(
                         url, "mstk", thread->conversation_turn_id);
                     url = net::AppendQueryParameter(url, "mtid",
                                                     thread->server_id);

                     std::move(callback).Run(url);
                   },
                   weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

GURL ContextualTasksUiService::GetDefaultAiPageUrl() {
  return lens::AppendCommonSearchParametersToURL(
      GURL(GetContextualTasksAiPageUrl()),
      g_browser_process->GetApplicationLocale(), false);
}

void ContextualTasksUiService::OnTaskChanged(
    BrowserWindowInterface* browser_window_interface,
    content::WebContents* web_contents,
    const base::Uuid& task_id,
    bool is_shown_in_tab) {
  if (!is_shown_in_tab && browser_window_interface) {
    // If a new thread is started in the panel, affiliated tabs should change
    // their thread to the new one.
    base::Uuid new_task_id = task_id;
    if (!task_id.is_valid()) {
      // If the panel is in zero state, create an empty task.
      ContextualTask task = contextual_tasks_service_->CreateTask();
      new_task_id = task.GetTaskId();
    }

    TabStripModel* tab_strip_model =
        browser_window_interface->GetTabStripModel();
    content::WebContents* active_contents =
        tab_strip_model->GetActiveWebContents();
    SessionID active_id = SessionTabHelper::IdForTab(active_contents);

    if (kTaskScopedSidePanel.Get()) {
      // If the current tab is associated with any task, change associations for
      // all tabs associated with that task.
      std::optional<ContextualTask> current_task =
          contextual_tasks_service_->GetContextualTaskForTab(active_id);
      if (current_task) {
        std::vector<SessionID> tab_ids =
            contextual_tasks_service_->GetTabsAssociatedWithTask(
                current_task->GetTaskId());
        for (const auto& id : tab_ids) {
          contextual_tasks_service_->AssociateTabWithTask(new_task_id, id);
        }
      }
    } else {
      contextual_tasks_service_->AssociateTabWithTask(new_task_id, active_id);
    }

    ContextualTasksSidePanelCoordinator* coordinator =
        ContextualTasksSidePanelCoordinator::From(browser_window_interface);
    coordinator->OnTaskChanged(web_contents, new_task_id);
  }
}

void ContextualTasksUiService::MoveTaskUiToNewTab(
    const base::Uuid& task_id,
    BrowserWindowInterface* browser,
    const GURL& inner_frame_url) {
  auto* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser);
  CHECK(coordinator);

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
        coordinator->DetachWebContentsForTask(task_id);
    if (!web_contents) {
      return;
    }

    content::WebUI* webui = web_contents->GetWebUI();

    NavigateParams params(browser, std::move(web_contents));
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.transition = ui::PAGE_TRANSITION_LINK;
    Navigate(&params);

    // Notify the WebUI that the tab status has changed only after the contents
    // has been moved to a tab.
    if (webui && webui->GetController()) {
      webui->GetController()
          ->GetAs<ContextualTasksUI>()
          ->OnSidePanelStateChanged();
    }
  }

  coordinator->Close();

  ContextualTasksService* task_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
          browser->GetProfile());
  CHECK(task_service);
  task_service->DisassociateAllTabsFromTask(task_id);
}

void ContextualTasksUiService::StartTaskUiInSidePanel(
    BrowserWindowInterface* browser_window_interface,
    tabs::TabInterface* tab_interface,
    const GURL& url,
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  CHECK(contextual_tasks_service_);

  // Get the coordinator for the current window.
  auto* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser_window_interface);
  auto* panel_contents = coordinator->GetActiveWebContents();

  // Create a task for the URL if the side panel wasn't already showing a task.
  if (!panel_contents || !coordinator->IsSidePanelOpenForContextualTask()) {
    ContextualTask task = contextual_tasks_service_->CreateTaskFromUrl(url);
    task_id_to_creation_url_[task.GetTaskId()] = url;
    AssociateWebContentsToTask(tab_interface->GetContents(), task.GetTaskId());
    coordinator->Show();

    // Associate the web contents with the task and set the session handle if
    // provided.
    content::WebContents* web_contents = coordinator->GetActiveWebContents();
    AssociateWebContentsToTask(web_contents, task.GetTaskId());
    if (session_handle) {
      ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents)
          ->SetTaskSession(task.GetTaskId(), std::move(session_handle));
    }
    return;
  }

  // If the side panel contents already exist, get the WebUI controller to
  // load the URL into the already loaded contextual tasks UI.
  if (panel_contents->GetWebUI()) {
    ContextualTasksUI* webui_controller = webui_controller =
        panel_contents->GetWebUI()->GetController()->GetAs<ContextualTasksUI>();
    content::OpenURLParams url_params(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
    webui_controller->TransferNavigationToEmbeddedPage(url_params);
  }
}

bool ContextualTasksUiService::IsAiUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      !net::SchemefulSite::IsSameSite(url, ai_page_host_)) {
    return false;
  }

  if (!base::StartsWith(url.path(), "/search")) {
    return false;
  }

  // AI pages are identified by the "udm" URL param having a value of "50" or
  // "nem" having a value of "143".
  std::string udm_value;
  if (net::GetValueForKeyInQuery(url, kUdmParam, &udm_value) &&
      udm_value == kUdmAiValue) {
    return true;
  }

  std::string nem_value;
  if (net::GetValueForKeyInQuery(url, kNemParam, &nem_value) &&
      nem_value == kNemAiValue) {
    return true;
  }

  return false;
}

bool ContextualTasksUiService::IsContextualTasksUrl(const GURL& url) {
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}

bool ContextualTasksUiService::IsValidSearchResultsPage(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      !net::SchemefulSite::IsSameSite(url, ai_page_host_)) {
    return false;
  }

  if (!base::StartsWith(url.path(), "/search")) {
    return false;
  }

  // Do not allow shopping mode queries.
  std::string value;
  if (net::GetValueForKeyInQuery(url, kUdmParam, &value) &&
      value == kShoppingModeParameterValue) {
    return false;
  }

  // The search results page is only valid if it has a text query or is a Lens
  // query.
  return (net::GetValueForKeyInQuery(url, kSearchQueryKey, &value) &&
          !value.empty()) ||
         (net::GetValueForKeyInQuery(url, kLensModeKey, &value) &&
          !value.empty());
}

void ContextualTasksUiService::OnLensOverlayStateChanged(
    BrowserWindowInterface* browser_window_interface,
    bool is_showing) {
  auto* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser_window_interface);
  if (!coordinator || !coordinator->IsSidePanelOpenForContextualTask()) {
    return;
  }

  auto* panel_contents = coordinator->GetActiveWebContents();
  if (!panel_contents || !panel_contents->GetWebUI()) {
    return;
  }

  auto* controller =
      panel_contents->GetWebUI()->GetController()->GetAs<ContextualTasksUI>();
  if (controller) {
    controller->OnLensOverlayStateChanged(is_showing);
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
  TabStripModel* tab_strip_model = browser->GetTabStripModel();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    tabs::TabInterface* tab_interface =
        tabs::TabInterface::GetFromContents(web_contents);
    if (tab_interface && tab_interface->GetHandle().raw_value() == tab_id &&
        web_contents->GetLastCommittedURL() == url) {
      tab_strip_model->ActivateTabAt(i);
      return;
    }
  }

  // The tab with the given ID and URL wasn't found. Next, try finding a tab
  // that matches the URL. If found, switch to it.
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    if (web_contents->GetLastCommittedURL() == url) {
      tab_strip_model->ActivateTabAt(i);
      return;
    }
  }

  // The tab wasn't found. Open a new tab with the given URL to the end of the
  // tab strip.
  NavigateParams params(browser->GetProfile(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}
}  // namespace contextual_tasks
