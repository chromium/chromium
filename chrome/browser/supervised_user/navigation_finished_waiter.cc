// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/navigation_finished_waiter.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

NavigationFinishedWaiter::NavigationFinishedWaiter(
    content::WebContents* web_contents,
    int frame_id,
    const GURL& url)
    : content::WebContentsObserver(web_contents),
      frame_id_(frame_id),
      url_(url) {}

NavigationFinishedWaiter::NavigationFinishedWaiter(
    content::WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents) {
  frame_id_ = web_contents->GetMainFrame()->GetFrameTreeNodeId();
  url_ = url;
}
void NavigationFinishedWaiter::Wait() {
  if (did_finish_)
    return;
  run_loop_.Run();
}

void NavigationFinishedWaiter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (navigation_handle->GetFrameTreeNodeId() != frame_id_ ||
      navigation_handle->GetURL() != url_)
    return;

  did_finish_ = true;
  run_loop_.Quit();
}
