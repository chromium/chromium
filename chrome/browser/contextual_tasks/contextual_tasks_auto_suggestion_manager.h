// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_AUTO_SUGGESTION_MANAGER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_AUTO_SUGGESTION_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace contextual_tasks {

// Information about a suggested tab to be shown in the composebox.
struct SuggestedTabInfo {
  SuggestedTabInfo();
  ~SuggestedTabInfo();
  SuggestedTabInfo(const SuggestedTabInfo&);

  int32_t tab_id = -1;
  std::u16string title;
  GURL url;
  base::TimeTicks last_active;
};

// Manages the state and logic for auto-suggestions in Contextual Tasks.
// This class is platform-agnostic and handles the blocklist of dismissed
// suggestions.
class ContextualTasksAutoSuggestionManager {
 public:
  ContextualTasksAutoSuggestionManager();
  ~ContextualTasksAutoSuggestionManager();

  ContextualTasksAutoSuggestionManager(
      const ContextualTasksAutoSuggestionManager&) = delete;
  ContextualTasksAutoSuggestionManager& operator=(
      const ContextualTasksAutoSuggestionManager&) = delete;

  // Updates the current candidate suggestion. If the suggestion is blocklisted,
  // it will be ignored and the current suggestion will be cleared.
  void SetCurrentSuggestion(std::unique_ptr<SuggestedTabInfo> info);

  // Called when the user explicitly adds a tab context (e.g. via the "+"
  // button). Removes the URL from the blocklist so it can be suggested again.
  void OnTabContextAdded(const GURL& url, bool is_active_tab);

  // Called when the user explicitly removes a manual tab context chip.
  // Adds the URL to the blocklist and clears the current suggestion if it
  // matches this URL.
  void OnTabContextRemoved(const GURL& url);

  // Called when the user dismisses the currently showing auto-suggestion chip.
  // Adds the URL to the blocklist and clears the current suggestion.
  void OnAutoSuggestionDismissed();

  // Clears all state (blocklist and current suggestion).
  void Reset();

  // Returns the current candidate suggestion, or nullptr if none or if it
  // is currently blocklisted.
  const SuggestedTabInfo* GetCurrentSuggestion() const;

  // Returns true if the given URL is in the blocklist.
  bool IsUrlBlocklisted(const GURL& url) const;

 private:
  // List of auto-suggested tab URLs that have been explicitly dismissed by the
  // user. Those URLs will not be auto-suggested again for the same task in the
  // same session, unless the user explicitly adds the tab via "+" button or
  // switches to a new thread in which case the whole list will be cleared.
  std::set<GURL> blocklisted_suggestions_;

  // The current candidate suggestion.
  std::unique_ptr<SuggestedTabInfo> current_suggestion_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_AUTO_SUGGESTION_MANAGER_H_
