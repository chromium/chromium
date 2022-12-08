// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action_recorder_tab_tracker.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action_recorder.h"

namespace app_list {

CrOSActionRecorderTabTracker::CrOSActionRecorderTabTracker(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CrOSActionRecorderTabTracker>(
          *web_contents) {}

// A tab should be skipped if it is empty, blank or default page.
bool CrOSActionRecorderTabTracker::ShouldSkip() {
  const std::string& url = web_contents()->GetURL().spec();
  return url.empty() || url == "about:blank" || url == "chrome://newtab/";
}

// For tracking tab navigations.
// TODO(https://crbug.com/1060819): Use this data with caution.
//   (1) Re-direction is counted as a navigation of a new tab.
//   (2) Preloading is counted as a navigation of a new tab.
void CrOSActionRecorderTabTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() || ShouldSkip()) {
    return;
  }

  const std::string& previous_url =
      navigation_handle->GetPreviousPrimaryMainFrameURL().spec();
  const std::string& url = web_contents()->GetURL().spec();

  // Only record when navigates to a new url.
  if (url != previous_url) {
    CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
        {base::StrCat({"TabNavigated-", url})},
        {{"Visibility", static_cast<int>(web_contents()->GetVisibility())},
         {"PageTransition",
          static_cast<int>(navigation_handle->GetPageTransition())}});
  }
}

// For tracking tab reactivations.
void CrOSActionRecorderTabTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE && !ShouldSkip()) {
    // Record a TabReactivated event.
    CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
        {base::StrCat({"TabReactivated-", web_contents()->GetURL().spec()})},
        {});
  }
}

// For tracking when a new url is opened from current tab.
void CrOSActionRecorderTabTracker::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (ShouldSkip())
    return;
  // Records that |url| is opened from current url.
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"TabOpened-", url.spec()})},
      {{"WindowOpenDisposition", static_cast<int>(disposition)},
       {web_contents()->GetURL().spec(), -1},
       {url.spec(), -2}});
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrOSActionRecorderTabTracker);

}  // namespace app_list
