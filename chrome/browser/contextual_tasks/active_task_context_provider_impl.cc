// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/common/url_constants.h"

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

  TabListInterface* tab_list = TabListInterface::From(browser_window);
  if (!tab_list) {
    return tabs;
  }

  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    tabs::TabInterface* tab = tab_list->GetTab(i);
    content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
    if (!web_contents) {
      continue;
    }
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
    if (context_session_ids.contains(tab_id)) {
      tabs.insert(tab->GetHandle());
    }
  }

  return tabs;
}

}  // namespace

// static
ActiveTaskContextProvider* ActiveTaskContextProvider::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

DEFINE_USER_DATA(ActiveTaskContextProvider);

ActiveTaskContextProviderImpl::ActiveTaskContextProviderImpl(
    BrowserWindowInterface* browser_window,
    ContextualTasksService* contextual_tasks_service)
    : browser_window_(browser_window),
      contextual_tasks_service_(contextual_tasks_service),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(contextual_tasks_service_);
}

void ActiveTaskContextProviderImpl::SetContextualTasksPanelController(
    ContextualTasksPanelController* contextual_tasks_panel_controller) {
  contextual_tasks_panel_controller_ = contextual_tasks_panel_controller;

  if (contextual_tasks_panel_controller_) {
    // Start observing the tab strip and ContextualTasksService.
    contextual_tasks_service_observation_.Observe(contextual_tasks_service_);
    auto* tab_list_interface = TabListInterface::From(browser_window_);
    if (tab_list_interface) {
      tab_list_interface->AddTabListInterfaceObserver(this);
      // Observe the active tab's WebContents on startup.
      OnActiveTabChanged(*tab_list_interface,
                         tab_list_interface->GetActiveTab());
    }
  }
}

ActiveTaskContextProviderImpl::~ActiveTaskContextProviderImpl() {
  if (auto* tab_list_interface = TabListInterface::From(browser_window_)) {
    tab_list_interface->RemoveTabListInterfaceObserver(this);
  }
}

void ActiveTaskContextProviderImpl::AddObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void ActiveTaskContextProviderImpl::RemoveObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ActiveTaskContextProviderImpl::OnActiveTabChanged(
    TabListInterface& tab_list,
    tabs::TabInterface* active_tab) {
  // Start observing the new active tab's WebContents.
  Observe(active_tab ? active_tab->GetContents() : nullptr);

  // Update the context based on the new active tab.
  RefreshContext();
}

void ActiveTaskContextProviderImpl::PrimaryPageChanged(content::Page& page) {
  // If we are navigating away from the AIM WebUI, disassociate the tab from the
  // task and clear its session handle.
  GURL url = page.GetMainDocument().GetLastCommittedURL();
  bool is_contextual_tasks_webui =
      url.scheme() == content::kChromeUIScheme &&
      url.host() == chrome::kChromeUIContextualTasksHost;

  if (!is_contextual_tasks_webui) {
    auto* helper =
        ContextualSearchWebContentsHelper::FromWebContents(web_contents());
    if (helper && helper->task_id()) {
      SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());
      contextual_tasks_service_->DisassociateTabFromTask(*helper->task_id(),
                                                         tab_id);
      helper->SetTaskSession(/*task_id=*/std::nullopt, /*handle=*/nullptr,
                             /*input_state_model=*/nullptr);
    }
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskAdded(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskUpdated(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskRemoved(
    const base::Uuid& task_id,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::RefreshContext() {
  // Increment the callback ID to invalidate any outstanding callbacks.
  callback_id_++;

  contextual_search::ContextualSearchSessionHandle* session_handle = nullptr;
  if (contextual_tasks_panel_controller_) {
    auto [task_id, handle] = contextual_tasks_panel_controller_
                                 ->GetSessionHandleForActiveTabOrSidePanel();
    session_handle = handle;
    active_task_id_ = task_id;
  } else {
    ResetStateAndNotifyObservers();
  }

  if (!active_task_id_.has_value()) {
    ResetStateAndNotifyObservers();
    return;
  }

  if (!session_handle) {
    ResetStateAndNotifyObservers();
    return;
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

  // Add auto-suggested tab if chip is showing.
  if (contextual_tasks_panel_controller_ &&
      contextual_tasks_panel_controller_->IsPanelOpenForContextualTask()) {
    auto maybe_handle =
        contextual_tasks_panel_controller_->GetAutoSuggestedTabHandle();
    if (maybe_handle) {
      tabs_to_underline.insert(*maybe_handle);
    }
  }

  for (auto& obs : observers_) {
    obs.OnContextTabsChanged(tabs_to_underline);
  }
}

void ActiveTaskContextProviderImpl::ResetStateAndNotifyObservers() {
  active_task_id_ = std::nullopt;
  for (auto& observer : observers_) {
    observer.OnContextTabsChanged({});
  }
}

}  // namespace contextual_tasks
