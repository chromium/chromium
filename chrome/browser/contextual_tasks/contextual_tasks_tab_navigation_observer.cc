// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_navigation_observer.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksTabNavigationObserver::ContextualTasksTabNavigationObserver(
    tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      will_discard_contents_subscription_(
          tab.RegisterWillDiscardContents(base::BindRepeating(
              &ContextualTasksTabNavigationObserver::OnWillDiscardContents,
              base::Unretained(this)))) {
  if (content::WebContents* contents = tab.GetContents()) {
    UpdateTaskForUrl(contents->GetLastCommittedURL());
  }
}

ContextualTasksTabNavigationObserver::~ContextualTasksTabNavigationObserver() =
    default;

void ContextualTasksTabNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // The AI Mode page updates `mtid` with same-document navigations.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    UpdateTaskForUrl(navigation_handle->GetURL());
  }
}

void ContextualTasksTabNavigationObserver::UpdateTaskForUrl(const GURL& url) {
  content::WebContents* contents = web_contents();
  if (!contents || !IsContextualTasksRearchitectureEnabled()) {
    return;
  }
  auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
      contents->GetBrowserContext());
  auto* service = ContextualTasksServiceFactory::GetForProfile(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  if (!ui_service || !service) {
    return;
  }

  // The AI Mode page has its thread's server id in `mtid`, except in zero
  // state and before the first response of a new thread.
  std::optional<std::string> thread_id;
  if (ui_service->IsAiUrl(url)) {
    thread_id = GetAiModeThreadId(url);
  }
  if (!thread_id) {
    if (thread_id_) {
      ClearThreadTask(contents);
      if (auto* user_data =
              ContextualTasksWebContentsUserData::FromWebContents(contents)) {
        user_data->RefreshActiveTaskContext();
      }
    }
    return;
  }

  std::optional<ContextualTask> task =
      service->GetTaskFromServerId(ThreadType::kAiMode, *thread_id);
  const base::Uuid task_id =
      task ? task->GetTaskId() : service->CreateTaskFromUrl(url).GetTaskId();
  UpdateAiModeThreadForTask(*service, task_id, *thread_id, url);
  if (thread_id_ == thread_id) {
    return;
  }

  ClearThreadTask(contents);
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(contents);
  // `task_id` adopts the unassigned session, created here if needed.
  user_data->GetOrCreateContextualSessionHandle();
  thread_id_ = thread_id;
  user_data->SetTaskId(task_id);
  service->AssociateTabWithTask(task_id,
                                sessions::SessionTabHelper::IdForTab(contents));
  user_data->RefreshActiveTaskContext();
}

void ContextualTasksTabNavigationObserver::ClearThreadTask(
    content::WebContents* contents) {
  if (!thread_id_) {
    return;
  }
  thread_id_.reset();
  if (!contents) {
    return;
  }
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(contents);
  std::optional<base::Uuid> task_id =
      user_data ? user_data->GetEffectiveTaskId() : std::nullopt;
  auto* service = ContextualTasksServiceFactory::GetForProfile(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  if (service && task_id) {
    service->DisassociateTabFromTask(
        *task_id, sessions::SessionTabHelper::IdForTab(contents));
  }
  if (user_data) {
    user_data->SetTaskId(std::nullopt);
  }
  if (auto* helper =
          ContextualSearchWebContentsHelper::FromWebContents(contents)) {
    helper->SetTaskSession(/*task_id=*/std::nullopt,
                           helper->TakeSessionHandle(),
                           helper->TakeInputStateModel());
  }
}

void ContextualTasksTabNavigationObserver::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  ClearThreadTask(old_contents);
  Observe(new_contents);
  if (new_contents) {
    UpdateTaskForUrl(new_contents->GetLastCommittedURL());
  }
}

}  // namespace contextual_tasks
