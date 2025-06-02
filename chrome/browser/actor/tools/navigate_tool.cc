// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool.h"

#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

using content::NavigationHandle;
using content::WebContents;

namespace actor {

NavigateTool::NavigateTool(WebContents& web_contents, const GURL& url)
    : WebContentsObserver(&web_contents), url_(url) {}

NavigateTool::~NavigateTool() = default;

void NavigateTool::Validate(ValidateCallback callback) {
  if (!url_.is_valid()) {
    // URL is invalid.
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kNavigateInvalidUrl));
    return;
  }

  // TODO(crbug.com/402731599): Validate URL and state here.

  PostResponseTask(std::move(callback), MakeOkResult());
}

void NavigateTool::Invoke(InvokeCallback callback) {
  content::OpenURLParams params(
      url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initiated */);

  CHECK(web_contents());

  invoke_callback_ = std::move(callback);

  // TODO(crbug.com/406545255): If the page has a BeforeUnload handler the user
  // may be prompted to confirm/abort the navigation, what should we do in those
  // cases?
  web_contents()->OpenURL(
      params, base::BindOnce(&NavigateTool::NavigationHandleCallback,
                             weak_ptr_factory_.GetWeakPtr()));
}

std::string NavigateTool::DebugString() const {
  return absl::StrFormat("NavigateTool[%s]", url_.spec());
}

ObservationDelayType NavigateTool::GetObservationDelayType() const {
  return ObservationDelayType::kWatchForLoad;
}

void NavigateTool::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // TODO(crbug.com/411748801): We should probably handle the case where the
  // page navigates before it's done loading. Common with client-side redirects.
  if (pending_navigation_handle_id_ &&
      navigation_handle->GetNavigationId() == *pending_navigation_handle_id_) {
    auto result =
        navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage()
            ? MakeOkResult()
            : MakeErrorResult();

    if (invoke_callback_) {
      PostResponseTask(std::move(invoke_callback_), std::move(result));
      return;
    }
  }
}

void NavigateTool::NavigationHandleCallback(NavigationHandle& handle) {
  pending_navigation_handle_id_ = handle.GetNavigationId();
}

}  // namespace actor
