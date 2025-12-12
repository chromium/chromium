// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"

namespace contextual_tasks {

namespace {

std::set<tabs::TabHandle> GetTabsFromContext(
    const ContextualTaskContext& context,
    BrowserWindowInterface* browser_window) {
  std::set<tabs::TabHandle> tabs;

  // Add the tabs from context if they exist in the current browser window.
  std::set<SessionID> context_session_ids;
  for (const auto& attachment : context.GetUrlAttachments()) {
    SessionID id = attachment.GetTabSessionId();
    if (id.is_valid()) {
      context_session_ids.insert(id);
    }
  }

  if (context_session_ids.empty()) {
    return tabs;
  }

  TabStripModel* tab_strip_model = browser_window->GetTabStripModel();
  if (!tab_strip_model) {
    return tabs;
  }

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    if (!web_contents) {
      continue;
    }
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
    if (context_session_ids.contains(tab_id)) {
      if (tabs::TabInterface* tab =
              tabs::TabInterface::GetFromContents(web_contents)) {
        tabs.insert(tab->GetHandle());
      }
    }
  }

  return tabs;
}

}  // namespace

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

void ActiveTaskContextProviderImpl::OnSidePanelStateUpdated(
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  // The side panel was just opened or closed or we might have switched to a
  // different tab. Update the context.
  active_session_handle_ =
      session_handle ? session_handle->AsWeakPtr() : nullptr;
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

void ActiveTaskContextProviderImpl::OnTaskRemoved(
    const base::Uuid& task_id,
    ContextualTasksService::TriggerSource source) {
  if (active_task_id_ == task_id) {
    // The task that was last shown was just removed. Refresh the tabs.
    active_task_id_ = std::nullopt;
    RefreshContext();
  }
}

void ActiveTaskContextProviderImpl::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  // Ignore the event if it is not for the task in the active tab.
  if (!active_task_id_ || active_task_id_ != task_id) {
    return;
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  // Ignore the event if it is not for the task in the active tab.
  if (!active_task_id_ || active_task_id_ != task_id) {
    return;
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::RefreshContext() {
  // Increment the callback ID to invalidate any outstanding callbacks.
  callback_id_++;

  if (!active_session_handle_) {
    ResetStateAndNotifyObservers();
    return;
  }

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

  auto* session_handle = active_session_handle_.get();

  bool session_handle_changed =
      last_session_id_ != session_handle->session_id();
  last_session_id_ = session_handle->session_id();

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

  std::set<tabs::TabHandle> tabs_to_underline =
      GetTabsFromContext(*context, browser_window_);

  // Add associated tabs.
  AddAssociatedTabsToSet(active_task_id_.value(), tabs_to_underline);

  for (auto& obs : observers_) {
    obs.OnContextTabsChanged(tabs_to_underline);
  }
}

void ActiveTaskContextProviderImpl::AddAssociatedTabsToSet(
    const base::Uuid& task_id,
    std::set<tabs::TabHandle>& tabs_to_underline) {
  TabStripModel* tab_strip_model = browser_window_->GetTabStripModel();

  // Add all associated tabs.
  for (const SessionID& tab_session_id :
       contextual_tasks_service_->GetTabsAssociatedWithTask(task_id)) {
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      CHECK(web_contents);
      if (tab_session_id !=
          sessions::SessionTabHelper::IdForTab(web_contents)) {
        continue;
      }

      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(web_contents);
      CHECK(tab);
      tabs_to_underline.insert(tab->GetHandle());
      break;
    }
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
