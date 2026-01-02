// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;

namespace contextual_tasks {
class ContextualTask;
struct ContextualTaskContext;

class ActiveTaskContextProviderImpl : public ActiveTaskContextProvider,
                                      public ContextualTasksService::Observer,
                                      public content::WebContentsObserver {
 public:
  explicit ActiveTaskContextProviderImpl(
      BrowserWindowInterface* browser_window,
      ContextualTasksService* contextual_tasks_service);
  ~ActiveTaskContextProviderImpl() override;

  ActiveTaskContextProviderImpl(const ActiveTaskContextProviderImpl&) = delete;
  ActiveTaskContextProviderImpl& operator=(
      const ActiveTaskContextProviderImpl&) = delete;

  // ActiveTaskContextProvider implementation.
  void OnSidePanelStateUpdated() override;
  void SetSessionHandleGetter(
      SessionHandleGetter session_handle_getter) override;
  void AddObserver(ActiveTaskContextProvider::Observer* observer) override;
  void RemoveObserver(ActiveTaskContextProvider::Observer* observer) override;

  // ContextualTasksService::Observer implementation.
  void OnTaskAdded(const ContextualTask& task,
                   ContextualTasksService::TriggerSource source) override;
  void OnTaskUpdated(const ContextualTask& task,
                     ContextualTasksService::TriggerSource source) override;
  void OnTaskRemoved(const base::Uuid& task_id,
                     ContextualTasksService::TriggerSource source) override;
  void OnTaskAssociatedToTab(const base::Uuid& task_id,
                             SessionID tab_id) override;
  void OnTaskDisassociatedFromTab(const base::Uuid& task_id,
                                  SessionID tab_id) override;
  void PrimaryPageChanged(content::Page& page) override;

 private:
  // Determines the active task and triggers a context fetch.
  void RefreshContext();

  // Callback for when GetContextForTask() completes.
  void OnGetContextForTask(int callback_id,
                           std::unique_ptr<ContextualTaskContext> context);

  void AddAssociatedTabsToSet(const base::Uuid& task_id,
                              std::set<tabs::TabHandle>& tabs_to_underline);
  void ResetStateAndNotifyObservers();
  void OnActiveTabChanged(BrowserWindowInterface* browser_window_interface);

  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<ContextualTasksService> contextual_tasks_service_;

  // Obtains session handle and task ID info about current tab.
  std::optional<SessionHandleGetter> session_handle_getter_;

  // The task associated with the currently active tab.
  std::optional<base::Uuid> active_task_id_;

  // An autoincrementing callback ID to filter out stale requests.
  int callback_id_ = 0;

  base::ObserverList<ActiveTaskContextProvider::Observer> observers_;

  // Scoped observation for contextual_tasks_service_.
  base::ScopedObservation<ContextualTasksService,
                          ContextualTasksService::Observer>
      contextual_tasks_service_observation_{this};

  // Subscription for tab switch events.
  base::CallbackListSubscription active_tab_change_subscription_;

  base::WeakPtrFactory<ActiveTaskContextProviderImpl> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_
