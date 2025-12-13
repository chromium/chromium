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
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {
class ContextualTask;
struct ContextualTaskContext;

class ActiveTaskContextProviderImpl : public ActiveTaskContextProvider,
                                      public ContextualTasksService::Observer {
 public:
  explicit ActiveTaskContextProviderImpl(
      BrowserWindowInterface* browser_window,
      ContextualTasksService* contextual_tasks_service);
  ~ActiveTaskContextProviderImpl() override;

  ActiveTaskContextProviderImpl(const ActiveTaskContextProviderImpl&) = delete;
  ActiveTaskContextProviderImpl& operator=(
      const ActiveTaskContextProviderImpl&) = delete;

  // ActiveTaskContextProvider implementation.
  void OnSidePanelStateUpdated(contextual_search::ContextualSearchSessionHandle*
                                   session_handle) override;
  void OnPendingContextUpdated(
      const contextual_search::ContextualSearchSessionHandle& session_handle)
      override;
  void AddObserver(ActiveTaskContextProvider::Observer* observer) override;
  void RemoveObserver(ActiveTaskContextProvider::Observer* observer) override;

  // ContextualTasksService::Observer implementation.
  void OnTaskUpdated(const ContextualTask& task,
                     ContextualTasksService::TriggerSource source) override;

 private:
  // Determines the active task and triggers a context fetch.
  void RefreshContext();

  // Callback for when GetContextForTask() completes.
  void OnGetContextForTask(int callback_id,
                           std::unique_ptr<ContextualTaskContext> context);

  void ResetStateAndNotifyObservers();

  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<ContextualTasksService> contextual_tasks_service_;

  // The task associated with the currently active tab.
  std::optional<base::Uuid> active_task_id_;

  // Handle to the compose plate of the active tab.
  std::optional<base::UnguessableToken> last_session_id_;

  // The active session handle from the side panel. If null, side panel is
  // closed.
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      active_session_handle_;

  // An autoincrementing callback ID to filter out stale requests.
  int callback_id_ = 0;

  base::ObserverList<ActiveTaskContextProvider::Observer> observers_;

  // Scoped observation for contextual_tasks_service_.
  base::ScopedObservation<ContextualTasksService,
                          ContextualTasksService::Observer>
      contextual_tasks_service_observation_{this};

  // Adds the tabs associated with the given `task_id` to `tabs_to_underline`.
  void AddAssociatedTabsToSet(const base::Uuid& task_id,
                              std::set<tabs::TabHandle>& tabs_to_underline);

  base::WeakPtrFactory<ActiveTaskContextProviderImpl> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_
