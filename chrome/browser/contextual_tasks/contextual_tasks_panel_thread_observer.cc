// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_thread_observer.h"

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

// Whether `url`, an AI Mode URL, has a turn: its latest turn (`mstk`) or its
// thread (`mtid`), which the server sets once a query is submitted. Unlike
// `mtid`, `mstk` is also set when signed out.
bool HasTurn(const GURL& url) {
  std::string mstk;
  return (net::GetValueForKeyInQuery(url, "mstk", &mstk) && !mstk.empty()) ||
         GetAiModeThreadId(url).has_value();
}

}  // namespace

ContextualTasksPanelThreadObserver::ContextualTasksPanelThreadObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualTasksPanelThreadObserver>(
          *web_contents),
      // A `WebContents` moved from a tab may already be on a thread, which
      // `ContextualTasksTabNavigationObserver` added to its task.
      thread_id_(GetAiModeThreadId(web_contents->GetLastCommittedURL())),
      has_turn_(HasTurn(web_contents->GetLastCommittedURL())) {
  // Safe to use `base::Unretained(this)` because `this` owns the subscription.
  zero_state_subscription_ =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents)
          ->RegisterZeroStateCallback(base::BindRepeating(
              &ContextualTasksPanelThreadObserver::OnZeroState,
              base::Unretained(this)));
}

ContextualTasksPanelThreadObserver::~ContextualTasksPanelThreadObserver() =
    default;

void ContextualTasksPanelThreadObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }
  content::WebContents* contents = web_contents();
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          contents->GetBrowserContext());
  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  const GURL& url = navigation_handle->GetURL();
  if (!ui_service || !service || !ui_service->IsAiUrl(url)) {
    return;
  }
  if (HasTurn(url)) {
    has_turn_ = true;
  }
  // A thread has no `mtid` until its first response, which the AI Mode page
  // adds with a same-document navigation. When signed out, it has no `mtid`.
  std::optional<std::string> thread_id = GetAiModeThreadId(url);
  std::optional<base::Uuid> current_task_id = GetCurrentTaskId();
  if (!thread_id || !current_task_id) {
    return;
  }

  // Like `ContextualTasksTabNavigationObserver`, the panel's task follows its
  // thread.
  base::Uuid task_id = *current_task_id;
  if (thread_id != thread_id_) {
    if (std::optional<ContextualTask> task =
            service->GetTaskFromServerId(ThreadType::kAiMode, *thread_id)) {
      task_id = task->GetTaskId();
    } else if (thread_id_) {
      // Another thread without a task, e.g. picked from history.
      task_id = service->CreateTaskFromUrl(url).GetTaskId();
    }
    // Otherwise, the panel had no thread yet (e.g. in zero state), so its
    // current task, with its attached tabs, gets the thread.
    thread_id_ = thread_id;
  }
  UpdateAiModeThreadForTask(*service, task_id, *thread_id, url);
  if (task_id != *current_task_id) {
    ChangeTask(*current_task_id, task_id);
  }
}

void ContextualTasksPanelThreadObserver::OnZeroState() {
  // The AI Mode page reported zero state after a turn, e.g. "New chat". Move to
  // a new task before the input state is updated for the new conversation and
  // before its first submit, so that neither uses the previous task. The
  // panel's initial zero state, without a turn, keeps its task.
  if (!has_turn_) {
    return;
  }
  content::WebContents* contents = web_contents();
  ContextualTasksService* service =
      ContextualTasksServiceFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  std::optional<base::Uuid> current_task_id = GetCurrentTaskId();
  if (!service || !current_task_id) {
    return;
  }
  thread_id_.reset();
  has_turn_ = false;
  ChangeTask(*current_task_id, service->CreateTask().GetTaskId());
}

std::optional<base::Uuid>
ContextualTasksPanelThreadObserver::GetCurrentTaskId() {
  // Like the submit, use the effective task, e.g. the full-tab task of a
  // `WebContents` moved from a tab.
  content::WebContents* contents = web_contents();
  std::optional<base::Uuid> task_id;
  if (ContextualTasksWebContentsUserData* user_data =
          ContextualTasksWebContentsUserData::FromWebContents(contents)) {
    task_id = user_data->GetEffectiveTaskId();
  } else if (ContextualSearchWebContentsHelper* helper =
                 ContextualSearchWebContentsHelper::FromWebContents(contents)) {
    task_id = helper->task_id();
  }
  if (!task_id || !task_id->is_valid()) {
    return std::nullopt;
  }
  return task_id;
}

void ContextualTasksPanelThreadObserver::ChangeTask(
    const base::Uuid& old_task_id,
    const base::Uuid& new_task_id) {
  content::WebContents* contents = web_contents();
  // A `WebContents` moved from a tab keeps its full-tab task in
  // `ContextualTasksWebContentsUserData`, which would take precedence over the
  // panel's task (see `GetEffectiveTaskId()`).
  if (ContextualTasksWebContentsUserData* user_data =
          ContextualTasksWebContentsUserData::FromWebContents(contents)) {
    user_data->SetTaskId(std::nullopt);
  }
  // Updates the panel's `ContextualSearchWebContentsHelper` task and session
  // and moves the tabs associated with `old_task_id` to `new_task_id`.
  if (ContextualTasksUiService* ui_service =
          ContextualTasksUiServiceFactory::GetForBrowserContext(
              contents->GetBrowserContext())) {
    ui_service->OnTaskChanged(webui::GetBrowserWindowInterface(contents),
                              contents, old_task_id, new_task_id,
                              /*is_shown_in_tab=*/false);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksPanelThreadObserver);

}  // namespace contextual_tasks
