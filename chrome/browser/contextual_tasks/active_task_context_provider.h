// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_H_

#include <optional>
#include <set>
#include <utility>

#include "base/observer_list_types.h"
#include "base/uuid.h"
#include "components/tabs/public/tab_interface.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

// Callback to obtain currently showing task's task ID and session handle.
using TaskAndSessionHandle =
    std::pair<std::optional<base::Uuid>,
              contextual_search::ContextualSearchSessionHandle*>;
using SessionHandleGetter = base::RepeatingCallback<TaskAndSessionHandle()>;

// A per-window context provider class that tracks the task associated with the
// active tab. It's responsible for providing info about which tabs are
// currently included in the context of the active tab's context and notifies
// the observers when the active tab is switched. Mainly used for underlining
// the tabs that are part of the active task.
class ActiveTaskContextProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the set of tabs that are part of the active context changes.
    virtual void OnContextTabsChanged(
        const std::set<tabs::TabHandle>& context_tabs) = 0;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called to notify the state of side panel on the active tab.
  // On receiving this signal, the provider is supposed to recompute the
  // context.
  // 1. After every tab switch with the correct state of side panel.
  // 2. Whenever the side panel is opened or closed, e.g. due to user action.
  virtual void OnSidePanelStateUpdated() = 0;

  // Sets the callback to be invoked to obtain the current task ID and session
  // handle. Must be invoked on startup.
  virtual void SetSessionHandleGetter(
      SessionHandleGetter session_handle_getter) = 0;

  virtual ~ActiveTaskContextProvider() = default;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_H_
