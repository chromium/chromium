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
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

class ContextualTasksPanelController;

// A per-window context provider class that tracks the task associated with the
// active tab. It's responsible for providing info about which tabs are
// currently included in the context of the active tab's context and notifies
// the observers when the active tab is switched. Explicitly used for
// underlining the tabs that are part of the active task.
class ActiveTaskContextProvider {
 public:
  DECLARE_USER_DATA(ActiveTaskContextProvider);

  class Observer : public base::CheckedObserver {
   public:
    // Called when the set of tabs that are part of the active context changes.
    virtual void OnContextTabsChanged(
        const std::set<tabs::TabHandle>& context_tabs) = 0;
  };

  static ActiveTaskContextProvider* From(BrowserWindowInterface* window);

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Must be invoked on startup to complete the dependency injection. Required
  // to be able to access current task, session handle and auto suggested chip
  // info.
  virtual void SetContextualTasksPanelController(
      ContextualTasksPanelController* contextual_tasks_panel_controller) = 0;

  // Central method called to recompute tab underlines based on the active task.
  // Called by various external callers (e.g. composebox, panel controller
  // etc). This is also the same method that gets invoked internally by the
  // implementation class in response to various observed events such as tab
  // switch, navigation, context update, tab association update etc.
  virtual void RefreshContext() = 0;

  virtual ~ActiveTaskContextProvider() = default;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ACTIVE_TASK_CONTEXT_PROVIDER_H_
