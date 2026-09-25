// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/google/core/common/google_util.h"
#include "components/rlz/rlz_tracker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

ChromeRLZTrackerWebContentsObserver::ChromeRLZTrackerWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

ChromeRLZTrackerWebContentsObserver::~ChromeRLZTrackerWebContentsObserver() =
    default;

// static
std::unique_ptr<ChromeRLZTrackerWebContentsObserver>
ChromeRLZTrackerWebContentsObserver::MaybeCreate(
    content::WebContents* web_contents) {
  if (rlz::RLZTracker::ShouldRecordChromeHomePageSearch()) {
    return base::WrapUnique(
        new ChromeRLZTrackerWebContentsObserver(web_contents));
  }
  return nullptr;
}

void ChromeRLZTrackerWebContentsObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (load_details.entry == nullptr) {
    return;
  }

  // Stop observing if we have recorded the search in other web contents.
  if (!rlz::RLZTracker::ShouldRecordChromeHomePageSearch()) {
    Observe(nullptr);
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

      // Stop observing since we only need to record the search once.
      Observe(nullptr);
    }
  }
}
