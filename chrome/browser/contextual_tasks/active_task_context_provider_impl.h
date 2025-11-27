// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_

#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
class BrowserWindowInterface;

namespace contextual_tasks {

class ActiveTaskContextProviderImpl : public ActiveTaskContextProvider {
 public:
  explicit ActiveTaskContextProviderImpl(
      BrowserWindowInterface* browser_window,
      ContextualTasksService* contextual_tasks_service);
  ~ActiveTaskContextProviderImpl() override;

  ActiveTaskContextProviderImpl(const ActiveTaskContextProviderImpl&) = delete;
  ActiveTaskContextProviderImpl& operator=(
      const ActiveTaskContextProviderImpl&) = delete;

  // ActiveTaskContextProvider implementation.
  void OnSidePanelStateUpdated(bool is_open) override;
  void AddObserver(ActiveTaskContextProvider::Observer* observer) override;
  void RemoveObserver(ActiveTaskContextProvider::Observer* observer) override;

 private:
  base::ObserverList<ActiveTaskContextProvider::Observer> observers_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_IMPL_H_
