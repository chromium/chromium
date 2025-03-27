// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool.h"

#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tab_collections/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using content::NavigationHandle;
using tabs::TabInterface;

namespace actor {

NavigateTool::NavigateTool(TabInterface& tab, const GURL& url)
    : WebContentsObserver(tab.GetContents()), url_(url) {}

NavigateTool::~NavigateTool() {
  CHECK(invoke_callback_.is_null());
}

void NavigateTool::Validate(ValidateCallback callback) {
  if (!url_.is_valid()) {
    // URL is invalid.
    PostResponseTask(std::move(callback), false);
    return;
  }

  // TODO(crbug.com/402731599): Validate URL and state here.

  PostResponseTask(std::move(callback), true);
}

void NavigateTool::Invoke(InvokeCallback callback) {
  content::OpenURLParams params(
      url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initiated */);

  CHECK(web_contents());

  invoke_callback_ = std::move(callback);

  web_contents()->OpenURL(
      params, base::BindOnce(&NavigateTool::NavigationHandleCallback,
                             weak_ptr_factory_.GetWeakPtr()));
}

void NavigateTool::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (pending_navigation_handle_id_ &&
      navigation_handle->GetNavigationId() == *pending_navigation_handle_id_) {
    CHECK(invoke_callback_);

    bool success =
        navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage();

    PostResponseTask(std::move(invoke_callback_), success);
  }
}

void NavigateTool::NavigationHandleCallback(NavigationHandle& handle) {
  pending_navigation_handle_id_ = handle.GetNavigationId();
}
}  // namespace actor
