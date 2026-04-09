// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace contextual_tasks {

// Information about a suggested tab to be shown in the composebox.
struct SuggestedTabInfo {
  int32_t tab_id;
  std::u16string title;
  GURL url;
  base::TimeTicks last_active;
};

// An interface for the composebox handler used by Contextual Tasks.
// This allows the WebUI to communicate with the composebox handler
// in a platform-agnostic way.
class ContextualTasksComposeboxHandlerInterface {
 public:
  virtual ~ContextualTasksComposeboxHandlerInterface() = default;

  // Resets the input state model to its initial state.
  virtual void ResetInputStateModel() = 0;

  // Called to clear the blocklist of auto-suggested tabs. This is used when
  // switching to a new thread.
  virtual void ResetBlocklistedSuggestions() = 0;

  // Called to update the suggested tab context chip in the compose box based on
  // the given candidate tab information. The chip will only be shown if the
  // candidate tab is eligible for suggestion and is not blocklisted by the
  // user. If `suggested_tab` is null, the suggested tab context is cleared.
  virtual void UpdateSuggestedTabContext(
      std::unique_ptr<SuggestedTabInfo> suggested_tab) = 0;

  // Notifies the handler that the active task has changed.
  virtual void OnTaskChanged() = 0;

  // Initializes the input state model.
  virtual void InitializeInputStateModel() = 0;

  // Updates the active model mode based on the given URL parameters.
  virtual void UpdateModelFromUrl(const GURL& url) = 0;

  // Returns true if there is a suggested tab context chip in the compose box.
  virtual bool has_suggested_tab_context() const = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_INTERFACE_H_
