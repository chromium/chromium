// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_tasks/public/contextual_task_context.h"

namespace contextual_tasks {

ActiveTaskContextProviderImpl::ActiveTaskContextProviderImpl(
    BrowserWindowInterface* browser_window,
    ContextualTasksService* contextual_tasks_service) {}

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
  // TODO(crbug.com/458718633): Implement it to account for all cases.
  for (auto& observer : observers_) {
    observer.OnContextTabsChanged({});
  }
}

}  // namespace contextual_tasks
