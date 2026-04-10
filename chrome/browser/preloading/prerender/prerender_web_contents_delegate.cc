// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_web_contents_delegate.h"

#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"

void PrerenderWebContentsDelegateImpl::PrerenderWebContentsCreated(
    content::WebContents* prerender_web_contents) {
  TabHelpers::AttachTabHelpers(prerender_web_contents);

  // Tag the prerender new tab contents so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForPrerenderNewTabContents(
      prerender_web_contents);
}

void PrerenderWebContentsDelegateImpl::PrerenderWebContentsReleased(
    content::WebContents* prerender_web_contents) {
  // Clear the prerender tag so the WebContents can be re-tagged as a regular
  // tab after activation.
  task_manager::WebContentsTags::ClearTag(prerender_web_contents);
}
