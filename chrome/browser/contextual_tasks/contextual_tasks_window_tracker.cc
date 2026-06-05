// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"

#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/common/logger.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

ContextualTasksWindowTracker::ContextualTasksWindowTracker(
    const ContextualTaskId& task_id,
    const GURL& expected_url,
    content::GlobalRenderFrameHostToken initiator_rfh_token,
    base::WeakPtr<content::WebContents> webui_contents,
    base::OnceCallback<void(base::WeakPtr<ContextualTasksWindowTracker>)>
        on_closed_callback)
    : task_id_(task_id),
      expected_url_(expected_url),
      initiator_rfh_token_(initiator_rfh_token),
      webui_contents_(webui_contents),
      on_closed_callback_(std::move(on_closed_callback)) {
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&ContextualTasksWindowTracker::OnWindowClosed,
                     base::Unretained(this)));
  OMNIBOX_LOG("window_tracker")
      << "ContextualTasksWindowTracker created for task: "
      << task_id_.value().AsLowercaseString();
}

ContextualTasksWindowTracker::~ContextualTasksWindowTracker() = default;

base::WeakPtr<content::WebContents>
ContextualTasksWindowTracker::initiator_contents() const {
  auto* rfh = GetInitiatorFrame();
  return rfh ? content::WebContents::FromRenderFrameHost(rfh)->GetWeakPtr()
             : nullptr;
}

content::RenderFrameHost* ContextualTasksWindowTracker::GetInitiatorFrame()
    const {
  return content::RenderFrameHost::FromFrameToken(initiator_rfh_token_);
}

void ContextualTasksWindowTracker::SetTabWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents->GetWeakPtr();
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  OMNIBOX_LOG("window_tracker")
      << "SetTabWebContents: tab found = " << (tab != nullptr);
  if (tab) {
    OnTabInterfaceAvailable(tab);
  }
}

void ContextualTasksWindowTracker::OnTabInterfaceAvailable(
    tabs::TabInterface* tab) {
  if (tab_) {
    return;
  }
  timeout_timer_.Stop();
  tab_ = tab;
  tab_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &ContextualTasksWindowTracker::OnTabWillDetach, GetWeakPtr()));
}

void ContextualTasksWindowTracker::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  OMNIBOX_LOG("window_tracker")
      << "OnTabWillDetach: reason = " << static_cast<int>(reason);
  OnWindowClosed();
  tab_ = nullptr;
  tab_subscription_ = {};
}

void ContextualTasksWindowTracker::OnWindowClosed() {
  OMNIBOX_LOG("window_tracker") << "OnWindowClosed called";
  if (on_closed_callback_) {
    // Post as a task to avoid deleting `this` synchronously if called during
    // observer notifications.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_closed_callback_), GetWeakPtr()));
  }
}

void ContextualTasksWindowTracker::SetOpenURLParams(
    const content::OpenURLParams& params) {
  open_url_params_ = std::make_unique<content::OpenURLParams>(params);
}

void ContextualTasksWindowTracker::SetMessageProxyWebContents(
    std::unique_ptr<content::WebContents> contents) {
  message_proxy_web_contents_ = std::move(contents);
}

const content::OpenURLParams* ContextualTasksWindowTracker::open_url_params()
    const {
  return open_url_params_.get();
}

}  // namespace contextual_tasks
