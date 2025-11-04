// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

using sessions::SessionTabHelper;

namespace contextual_tasks {

namespace {
constexpr char kAiPageHost[] = "https://google.com";
constexpr char kTaskQueryParam[] = "task";

bool IsContextualTasksHost(const GURL& url) {
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}

GURL AppendCommonUrlParams(GURL url) {
  url = net::AppendQueryParameter(url, "gsc", "2");
  // TODO(crbug.com/454388385): Remove this param once authentication flow is
  // implemented.
  url = net::AppendQueryParameter(url, "gl", "us");
  return url;
}

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
}  // namespace

ContextualTasksUiService::ContextualTasksUiService(
    Profile* profile,
    ContextualTasksContextController* context_controller)
    : profile_(profile), context_controller_(context_controller) {
  ai_page_host_ = GURL(kAiPageHost);
}

ContextualTasksUiService::~ContextualTasksUiService() = default;

void ContextualTasksUiService::OnNavigationToAiPageIntercepted(
    const GURL& url,
    base::WeakPtr<tabs::TabInterface> tab,
    bool is_to_new_tab) {
  CHECK(context_controller_);

  // Create a task for the URL that was just intercepted.
  ContextualTask task = context_controller_->CreateTaskFromUrl(url);

  // Map the task ID to the a new URL that uses the base AI page URL with the
  // query from the one that was intercepted. This is done so the UI knows
  // which URL to load initially in the embedded frame.
  std::string query;
  net::GetValueForKeyInQuery(url, "q", &query);
  GURL stripped_query_url = GetDefaultAiPageUrl();
  if (!query.empty()) {
    stripped_query_url =
        net::AppendQueryParameter(stripped_query_url, "q", query);
  }
  task_id_to_creation_url_[task.GetTaskId()] = stripped_query_url;

    GURL ui_url(chrome::kChromeUIContextualTasksURL);
  ui_url = net::AppendQueryParameter(ui_url, kTaskQueryParam,
                                     task.GetTaskId().AsLowercaseString());

  content::WebContents* contextual_task_web_contents = nullptr;
  if (!is_to_new_tab) {
    tab->GetContents()->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(ui_url));
    contextual_task_web_contents = tab->GetContents();
  } else {
    NavigateParams params(profile_, ui_url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

    Navigate(&params);
    contextual_task_web_contents = params.navigated_or_inserted_contents;
  }
  // Attach the session Id of the ai page to the task.
  if (contextual_task_web_contents) {
    AssociateWebContentsToTask(contextual_task_web_contents, task.GetTaskId());
  }
}

void ContextualTasksUiService::OnThreadLinkClicked(
    const GURL& url,
    base::Uuid task_id,
    base::WeakPtr<tabs::TabInterface> tab) {
  // If the source contents is the panel, open the AI page in a new foreground
  // tab.
  if (!tab) {
    NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);

    // TODO(crbug.com/453025914): Consider moving the newly created tab next to
    //    the tab that is responsible for creating it if the AI page is in tab
    //    mode.
    Navigate(&params);

    // Associate the new tab's WebContents to the task.
    // TODO(crbug.com/449161768): this could happen before the tab is created.
    // We might need to create the tab in the background and attach it later, or
    // we need to observe the WebContents lifecycle here.
    content::WebContents* new_tab_web_contents =
        params.navigated_or_inserted_contents;
    if (new_tab_web_contents && task_id.is_valid()) {
      AssociateWebContentsToTask(new_tab_web_contents, task_id);
    }
    return;
  }

  BrowserWindowInterface* browser_window_interface =
      tab->GetBrowserWindowInterface();
  TabStripModel* tab_strip_model = browser_window_interface->GetTabStripModel();

  // Get the index of the web contents.
  const int current_index = tab_strip_model->GetIndexOfTab(tab.get());

  // Open the linked page in a tab directly after this one.
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_));
  content::WebContents* new_contents_ptr = new_contents.get();
  new_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  tab_strip_model->InsertWebContentsAt(
      current_index + 1, std::move(new_contents), AddTabTypes::ADD_ACTIVE);

  // Detach the WebContents from tab.
  std::unique_ptr<content::WebContents> contextual_task_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(current_index);

  CHECK(new_contents_ptr == tab_strip_model->GetActiveWebContents());
  SessionID session_id = SessionTabHelper::IdForTab(new_contents_ptr);
  context_controller_->AssociateTabWithTask(task_id, session_id);

  // Transfer the contextual task contents into the side panel cache.
  ContextualTasksSidePanelCoordinator::From(browser_window_interface)
      ->TransferWebContentsFromTab(task_id,
                                   std::move(contextual_task_contents));

  // Open the side panel.
  // TODO: This currently should be passed the bounds of the
  // contents_container_view from BrowserView, though the view is not accessible
  // from here. This API could be changed to simply accept the web_contents.
  ContextualTasksSidePanelCoordinator::From(browser_window_interface)->Show();
}

bool ContextualTasksUiService::HandleNavigation(
    const GURL& navigation_url,
    const GURL& responsible_web_contents_url,
    const content::FrameTreeNodeId& source_frame_tree_node_id,
    bool is_to_new_tab) {
  // Allow any navigation to the contextual tasks host.
  if (IsContextualTasksHost(navigation_url)) {
    return false;
  }

  bool is_nav_to_ai = IsAiUrl(navigation_url);
  bool is_nav_to_sign_in = IsSignInDomain(navigation_url);

  // Try to get the active tab if there is one. This will be null if the link is
  // originating from the side panel.
  content::WebContents* source_contents =
      content::WebContents::FromFrameTreeNodeId(source_frame_tree_node_id);
  tabs::TabInterface* tab = nullptr;
  if (source_contents) {
    tab = tabs::TabInterface::MaybeGetFromContents(source_contents);
  }

  // Intercept any navigation where the wrapping WebContents is the WebUI host
  // unless it is the AI page.
  if (IsContextualTasksHost(responsible_web_contents_url)) {
    if (is_nav_to_ai) {
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
      task_id = GetTaskIdFromHostURL(source_contents->GetURL());
    }

    // This needs to be posted in case the called method triggers a navigation
    // in the same WebContents, invalidating the nav handle used up the chain.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksUiService::OnThreadLinkClicked,
                       weak_ptr_factory_.GetWeakPtr(), navigation_url, task_id,
                       tab ? tab->GetWeakPtr() : nullptr));
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
            weak_ptr_factory_.GetWeakPtr(), navigation_url,
            tab ? tab->GetWeakPtr() : nullptr, is_to_new_tab));
    return true;
  }

  // Allow anything else.
  return false;
}

GURL ContextualTasksUiService::GetInitialUrlForTask(const base::Uuid& uuid) {
  auto it = task_id_to_creation_url_.find(uuid);
  if (it != task_id_to_creation_url_.end()) {
    return it->second;
  }
  return GURL();
}

GURL ContextualTasksUiService::GetDefaultAiPageUrl() {
  return AppendCommonUrlParams(GURL(GetContextualTasksAiPageUrl()));
}

bool ContextualTasksUiService::IsAiUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      !base::EndsWith(url.host(), ai_page_host_.host())) {
    return false;
  }

  if (!base::StartsWith(url.path(), "/search")) {
    return false;
  }

  // AI pages are identified by the "udm" URL param having a value of 50.
  std::string udm_value;
  if (!net::GetValueForKeyInQuery(url, "udm", &udm_value)) {
    return false;
  }

  return udm_value == "50";
}

void ContextualTasksUiService::AssociateWebContentsToTask(
    content::WebContents* web_contents,
    const base::Uuid& task_id) {
  SessionID session_id = SessionTabHelper::IdForTab(web_contents);
  if (session_id.is_valid()) {
    context_controller_->AssociateTabWithTask(task_id, session_id);
  }
}
}  // namespace contextual_tasks
