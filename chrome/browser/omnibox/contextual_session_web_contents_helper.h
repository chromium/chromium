// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_WEB_CONTENTS_HELPER_H_
#define CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_WEB_CONTENTS_HELPER_H_

#include <memory>

#include "components/omnibox/composebox/contextual_session_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Helper class that scopes the `ContextualSessionService::SessionHandle`'s
// lifetime to a `content::WebContents`.
class ContextualSessionWebContentsHelper
    : public content::WebContentsUserData<ContextualSessionWebContentsHelper> {
 public:
  ContextualSessionWebContentsHelper(
      const ContextualSessionWebContentsHelper&) = delete;
  ContextualSessionWebContentsHelper& operator=(
      const ContextualSessionWebContentsHelper&) = delete;
  ~ContextualSessionWebContentsHelper() override;

  // Takes ownership of a contextual session handle.
  void set_session_handle(
      std::unique_ptr<ContextualSessionService::SessionHandle> handle) {
    session_handle_ = std::move(handle);
  }
  // Returns the owned contextual session handle. May return nullptr.
  ContextualSessionService::SessionHandle* session_handle() const {
    return session_handle_.get();
  }

 private:
  explicit ContextualSessionWebContentsHelper(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextualSessionWebContentsHelper>;

  std::unique_ptr<ContextualSessionService::SessionHandle> session_handle_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_WEB_CONTENTS_HELPER_H_
