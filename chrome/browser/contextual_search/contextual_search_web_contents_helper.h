// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/uuid.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Helper class that scopes the `ContextualSearchSessionHandle`'s
// lifetime to a `content::WebContents`. Used for transferring a contextual
// search session from one WebUI (e.g., Omnibox) to another (i.e., Co-Browsing)
// when the user submits a query.
class ContextualSearchWebContentsHelper
    : public content::WebContentsUserData<ContextualSearchWebContentsHelper> {
 public:
  ContextualSearchWebContentsHelper(const ContextualSearchWebContentsHelper&) =
      delete;
  ContextualSearchWebContentsHelper& operator=(
      const ContextualSearchWebContentsHelper&) = delete;
  ~ContextualSearchWebContentsHelper() override;

  // Sets the task ID and the contextual search session handle for the task.
  // `task_id` can be std::nullopt when transferring session before task
  // assignment.
  void SetTaskSession(
      std::optional<base::Uuid> task_id,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          handle) {
    task_id_ = task_id;
    session_handle_ = std::move(handle);
  }
  // Returns the contextual search session handle. May return nullptr.
  contextual_search::ContextualSearchSessionHandle* session_handle() const {
    return session_handle_.get();
  }

  // Returns the task ID associated with the current contextual search session.
  // std::nullopt if the web_contents isn't showing a contextual task.
  const std::optional<base::Uuid>& task_id() const { return task_id_; }

  // Returns contextual search session handle only if it matches `task_id`.
  // Returns nullptr if no session exists or `task_id` doesn't match.
  // This will update the task ID to be `task_id` if it was previously empty.
  contextual_search::ContextualSearchSessionHandle* GetSessionForTask(
      const base::Uuid& task_id) {
    if (!task_id_) {
      task_id_ = std::make_optional(task_id);
    }
    return (session_handle_ && task_id_ == task_id) ? session_handle_.get()
                                                    : nullptr;
  }

 private:
  explicit ContextualSearchWebContentsHelper(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextualSearchWebContentsHelper>;

  // The task ID the session handle is associated with, if any.
  std::optional<base::Uuid> task_id_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_
