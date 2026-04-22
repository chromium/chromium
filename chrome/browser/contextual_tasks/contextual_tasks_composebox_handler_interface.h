// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_auto_suggestion_manager.h"
#include "url/gurl.h"

namespace contextual_tasks {

// An interface for the composebox handler used by Contextual Tasks.
// This allows the WebUI to communicate with the composebox handler
// in a platform-agnostic way.
class ContextualTasksComposeboxHandlerInterface {
 public:
  virtual ~ContextualTasksComposeboxHandlerInterface() = default;

  // Resets the input state model to its initial state.
  virtual void ResetInputStateModel() = 0;

  // Called to update the suggested tab context chip in the compose box based on
  // the given candidate tab information. The caller is responsible for ensuring
  // the provided `suggested_tab` has been filtered against the blocklist (via
  // the AutoSuggestionManager). If `suggested_tab` is null, the suggested tab
  // context is cleared.
  virtual void UpdateSuggestedTabContext(
      const SuggestedTabInfo* suggested_tab) = 0;

  // Notifies the handler that the active task has changed.
  virtual void OnTaskChanged() = 0;

  // Initializes the input state model.
  virtual void InitializeInputStateModel() = 0;

  // Updates the active model mode based on the given URL parameters.
  virtual void UpdateModelFromUrl(const GURL& url) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_
