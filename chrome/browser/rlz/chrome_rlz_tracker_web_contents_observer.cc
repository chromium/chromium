// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"

#include "components/google/core/common/google_util.h"
#include "components/rlz/rlz_tracker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"

ChromeRLZTrackerWebContentsObserver::ChromeRLZTrackerWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeRLZTrackerWebContentsObserver>(
          *web_contents) {}

ChromeRLZTrackerWebContentsObserver::~ChromeRLZTrackerWebContentsObserver() =
    default;

// static
void ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
    content::WebContents* web_contents) {
  if (rlz::RLZTracker::ShouldRecordChromeHomePageSearch()) {
    CreateForWebContents(web_contents);
  }
}

void ChromeRLZTrackerWebContentsObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (load_details.entry == nullptr) {
    return;
  }

  // Remove the observer if we have recorded the search in other web contents.
  if (!rlz::RLZTracker::ShouldRecordChromeHomePageSearch()) {
    web_contents()->RemoveUserData(UserDataKey());
    return;
  }

  // Firstly check if it is a Google search.
  if (google_util::IsGoogleSearchUrl(load_details.entry->GetURL())) {
    // If it is a Google search, check if it originates from HOMEPAGE by getting
    // the previous NavigationEntry.
    int entry_index =
        web_contents()->GetController().GetLastCommittedEntryIndex();
    if (entry_index < 1) {
      return;
    }

    content::NavigationEntry* previous_entry =
        web_contents()->GetController().GetEntryAtIndex(entry_index - 1);
    if (previous_entry == nullptr) {
      return;
    }

    // Make sure it is a Google web page originated from HOMEPAGE.
    if (google_util::IsGoogleHomePageUrl(previous_entry->GetURL()) &&
        ((previous_entry->GetTransitionType() &
          ui::PAGE_TRANSITION_HOME_PAGE) != 0)) {
      rlz::RLZTracker::RecordChromeHomePageSearch();

      // Remove the observer since we only need to record the search once.
      web_contents()->RemoveUserData(UserDataKey());
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeRLZTrackerWebContentsObserver);
