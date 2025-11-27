// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"

namespace contextual_tasks {

ActiveTaskContextProviderImpl::ActiveTaskContextProviderImpl(
    BrowserWindowInterface* browser_window,
    ContextualTasksService* contextual_tasks_service)
    : browser_window_(browser_window),
      contextual_tasks_service_(contextual_tasks_service) {
  CHECK(contextual_tasks_service_);
  contextual_tasks_service_observation_.Observe(contextual_tasks_service_);
}

ActiveTaskContextProviderImpl::~ActiveTaskContextProviderImpl() = default;

void ActiveTaskContextProviderImpl::AddObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void ActiveTaskContextProviderImpl::RemoveObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ActiveTaskContextProviderImpl::OnSidePanelStateUpdated(bool is_open) {
  // The side panel was just opened or closed or we might have switched to a
  // different tab. Update the context.
  is_side_panel_open_ = is_open;
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnPendingContextUpdated(
    const contextual_search::ContextualSearchSessionHandle& session_handle) {
  // Ignore the update if it is not for the task in the active tab.
  if (last_session_id_ != session_handle.session_id()) {
    return;
  }
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskUpdated(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  // Ignore the update if it is not for the task in the active tab.
  if (!active_task_id_ || active_task_id_ != task.GetTaskId()) {
    return;
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::RefreshContext() {
  // Increment the callback ID to invalidate any outstanding callbacks.
  callback_id_++;

  tabs::TabInterface* active_tab = browser_window_->GetActiveTabInterface();
  if (!active_tab) {
    ResetStateAndNotifyObservers();
    return;
  }

  content::WebContents* web_contents = active_tab->GetContents();
  if (!web_contents) {
    ResetStateAndNotifyObservers();
    return;
  }

  auto* session_handle_tab_helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  auto* session_handle = session_handle_tab_helper
                             ? session_handle_tab_helper->session_handle()
                             : nullptr;
  if (!session_handle) {
    ResetStateAndNotifyObservers();
    return;
  }

  bool session_handle_changed =
      last_session_id_ != session_handle->session_id();
  last_session_id_ = session_handle->session_id();

  if (!is_side_panel_open_) {
    ResetStateAndNotifyObservers();
    return;
  }

  auto task = contextual_tasks_service_->GetContextualTaskForTab(
      sessions::SessionTabHelper::IdForTab(web_contents));
  active_task_id_ =
      task.has_value() ? std::make_optional(task->GetTaskId()) : std::nullopt;

  if (!active_task_id_.has_value()) {
    ResetStateAndNotifyObservers();
    return;
  }

  if (session_handle_changed) {
    // The session handle has changed. The observers need to reset their state
    // first.
    for (auto& observer : observers_) {
      observer.OnContextTabsChanged({});
    }
  }

  auto context_decoration_params = std::make_unique<ContextDecorationParams>();
  context_decoration_params->contextual_search_session_handle =
      session_handle->AsWeakPtr();
  contextual_tasks_service_->GetContextForTask(
      active_task_id_.value(), {}, std::move(context_decoration_params),
      base::BindOnce(&ActiveTaskContextProviderImpl::OnGetContextForTask,
                     weak_ptr_factory_.GetWeakPtr(), callback_id_));
}

void ActiveTaskContextProviderImpl::OnGetContextForTask(
    int callback_id,
    std::unique_ptr<ContextualTaskContext> context) {
  // Ignore stale results if a newer request has been sent.
  if (callback_id != callback_id_) {
    return;
  }

  if (!context) {
    return;
  }

  // TODO(shaktisahu): Retrieve the tab handles from `context`.
  std::set<tabs::TabHandle> context_tabs;
  for (auto& obs : observers_) {
    obs.OnContextTabsChanged(context_tabs);
  }
}

void ActiveTaskContextProviderImpl::ResetStateAndNotifyObservers() {
  active_task_id_ = std::nullopt;
  last_session_id_ = std::nullopt;
  for (auto& observer : observers_) {
    observer.OnContextTabsChanged({});
  }
}

}  // namespace contextual_tasks
