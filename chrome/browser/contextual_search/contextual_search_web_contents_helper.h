// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_

#include <memory>

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

  // Takes ownership of a contextual session handle and stores it.
  void set_session_handle(
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          handle) {
    session_handle_ = std::move(handle);
  }
  // Returns the owned contextual session handle. May return nullptr.
  contextual_search::ContextualSearchSessionHandle* session_handle() const {
    return session_handle_.get();
  }
  // Takes ownership away from this helper and returns it.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
  TakeSessionHandle() {
    return std::move(session_handle_);
  }

 private:
  explicit ContextualSearchWebContentsHelper(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextualSearchWebContentsHelper>;

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_WEB_CONTENTS_HELPER_H_
