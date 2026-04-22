// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_auto_suggestion_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace contextual_tasks {

SuggestedTabInfo::SuggestedTabInfo() = default;
SuggestedTabInfo::~SuggestedTabInfo() = default;
SuggestedTabInfo::SuggestedTabInfo(const SuggestedTabInfo&) = default;

ContextualTasksAutoSuggestionManager::ContextualTasksAutoSuggestionManager() =
    default;
ContextualTasksAutoSuggestionManager::~ContextualTasksAutoSuggestionManager() =
    default;

void ContextualTasksAutoSuggestionManager::SetCurrentSuggestion(
    std::unique_ptr<SuggestedTabInfo> info) {
  if (!info) {
    current_suggestion_ = nullptr;
    return;
  }

  if (IsUrlBlocklisted(info->url)) {
    current_suggestion_ = nullptr;
    return;
  }

  current_suggestion_ = std::move(info);
}

void ContextualTasksAutoSuggestionManager::OnTabContextAdded(
    const GURL& url,
    bool is_active_tab) {
  if (is_active_tab && !blocklisted_suggestions_.empty()) {
    const std::string metric_name =
        "ContextualTasks.Composebox.UserAction."
        "AddedActiveTabAfterDeletingAutoSuggestion";
    base::UmaHistogramBoolean(metric_name, true);
    base::RecordAction(base::UserMetricsAction(metric_name.c_str()));
  }
  blocklisted_suggestions_.erase(url);
}

void ContextualTasksAutoSuggestionManager::OnTabContextRemoved(
    const GURL& url) {
  blocklisted_suggestions_.insert(url);
  // If the removed URL matches the current candidate, clear it.
  if (current_suggestion_ && current_suggestion_->url == url) {
    current_suggestion_ = nullptr;
  }
}

void ContextualTasksAutoSuggestionManager::OnAutoSuggestionDismissed() {
  if (current_suggestion_) {
    GURL url = current_suggestion_->url;
    // Set to null first to invalidate the current suggestion.
    current_suggestion_ = nullptr;
    blocklisted_suggestions_.insert(url);
  }
}

void ContextualTasksAutoSuggestionManager::Reset() {
  blocklisted_suggestions_.clear();
  current_suggestion_ = nullptr;
}

const SuggestedTabInfo*
ContextualTasksAutoSuggestionManager::GetCurrentSuggestion() const {
  return current_suggestion_.get();
}

bool ContextualTasksAutoSuggestionManager::IsUrlBlocklisted(
    const GURL& url) const {
  return blocklisted_suggestions_.contains(url);
}

}  // namespace contextual_tasks
