// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool.h"

#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace actor {

using ::content::NavigationController;
using ::content::NavigationHandle;
using ::tabs::TabInterface;

HistoryTool::HistoryTool(TabInterface& tab, Direction direction)
    : WebContentsObserver(tab.GetContents()), direction_(direction) {
  CHECK(tab.GetContents());
}

HistoryTool::~HistoryTool() = default;

void HistoryTool::Validate(ValidateCallback callback) {
  NavigationController& controller = web_contents()->GetController();
  if ((direction_ == kBack && !controller.CanGoBack()) ||
      (direction_ == kForward && !controller.CanGoForward())) {
    PostResponseTask(std::move(callback), false);
    return;
  }

  // TODO(crbug.com/402731599): Additional validation here (e.g. is URL in
  // allowlist).

  PostResponseTask(std::move(callback), true);
}

void HistoryTool::Invoke(InvokeCallback callback) {
  CHECK(web_contents());
  CHECK(!is_collecting_new_navigations_);

  invoke_callback_ = std::move(callback);

  // TODO(crbug.com/406545255): A navigation resulting in BeforeUnload being
  // executed isn't started until it is asynchronously resolved resulting in a
  // navigation. It's not clear yet what to do in these cases.
  {
    // Track any new navigations started as a result of GoBack/GoForward.
    is_collecting_new_navigations_ = true;
    if (direction_ == kBack) {
      web_contents()->GetController().GoBack();
    } else {
      CHECK_EQ(direction_, kForward);
      web_contents()->GetController().GoForward();
    }
  }

  // Despite the above TODO about not handling BeforeUnload, navigation code has
  // an async browser-based before unload step even if there is no beforeunload
  // handler (this is the `is_legacy` case in SendBeforeUnload). Post a task
  // to the same queue that uses to ensure that a navigation will have started
  // by then.
  content::GetUIThreadTaskRunner(
      {content::BrowserTaskType::kBeforeUnloadBrowserResponse})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &HistoryTool::LegacyBrowserBasedBeforeUnloadReplyComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HistoryTool::DidStartNavigation(NavigationHandle* navigation_handle) {
  if (!is_collecting_new_navigations_ || !navigation_handle->IsHistory()) {
    return;
  }

  navigation_handle_ids_.insert(navigation_handle->GetNavigationId());
}

void HistoryTool::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (navigation_handle_ids_.erase(navigation_handle->GetNavigationId())) {
    bool success =
        navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage();
    FinishToolInvocationIfNeeded(success);
  }
}

void HistoryTool::FinishToolInvocationIfNeeded(bool result) {
  CHECK(invoke_callback_);
  // This responds with failure if any navigations fails.
  if (navigation_handle_ids_.empty() || !result) {
    PostResponseTask(std::move(invoke_callback_), result);
  }
}

void HistoryTool::LegacyBrowserBasedBeforeUnloadReplyComplete() {
  is_collecting_new_navigations_ = false;

  // If no navigations were started, we should complete now. Respond with
  // failure since nothing changed.
  if (navigation_handle_ids_.empty()) {
    FinishToolInvocationIfNeeded(false);
  }
}

}  // namespace actor
