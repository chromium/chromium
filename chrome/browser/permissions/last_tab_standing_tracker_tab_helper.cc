// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/last_tab_standing_tracker_tab_helper.h"

#include "chrome/browser/permissions/last_tab_standing_tracker.h"
#include "chrome/browser/permissions/last_tab_standing_tracker_factory.h"
#include "content/public/browser/navigation_handle.h"

LastTabStandingTrackerTabHelper::~LastTabStandingTrackerTabHelper() = default;

void LastTabStandingTrackerTabHelper::WebContentsDestroyed() {
  if (last_committed_origin_) {
    LastTabStandingTrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext())
        ->WebContentsUnloadedOrigin(*last_committed_origin_);
  }
}

void LastTabStandingTrackerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  url::Origin new_origin =
      web_contents()->GetMainFrame()->GetLastCommittedOrigin();
  if (last_committed_origin_ && *last_committed_origin_ == new_origin)
    return;
  auto* last_tab_standing_tracker =
      LastTabStandingTrackerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (last_committed_origin_) {
    last_tab_standing_tracker->WebContentsUnloadedOrigin(
        *last_committed_origin_);
  }
  last_tab_standing_tracker->WebContentsLoadedOrigin(new_origin);
  last_committed_origin_ = std::move(new_origin);
}

LastTabStandingTrackerTabHelper::LastTabStandingTrackerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LastTabStandingTrackerTabHelper)
