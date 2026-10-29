// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"

#include "content/public/browser/web_contents.h"

ContextualSearchWebContentsHelper::ContextualSearchWebContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ContextualSearchWebContentsHelper>(
          *web_contents) {}

ContextualSearchWebContentsHelper::~ContextualSearchWebContentsHelper() =
    default;

std::unique_ptr<contextual_search::InputStateModel>
ContextualSearchWebContentsHelper::TakeInputStateModel() {
  return std::move(input_state_model_);
}

base::CallbackListSubscription
ContextualSearchWebContentsHelper::RegisterSessionChangedCallback(
    SessionChangedCallbackList::CallbackType cb) {
  return session_changed_callbacks_.Add(std::move(cb));
}

std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
ContextualSearchWebContentsHelper::TakeSessionHandle() {
  auto handle = std::move(session_handle_);
  if (handle) {
    session_changed_callbacks_.Notify(nullptr);
  }
  return handle;
}

const std::vector<int32_t>&
ContextualSearchWebContentsHelper::GetSelectedTabIds() const {
  return selected_tab_ids_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualSearchWebContentsHelper);
