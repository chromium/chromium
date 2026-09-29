// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chained_back_navigation_tracker.h"

#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-shared.h"
#include "ui/base/page_transition_types.h"

DEFINE_USER_DATA(ChainedBackNavigationTracker);

ChainedBackNavigationTracker::ChainedBackNavigationTracker(
    tabs::TabInterface& tab,
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

ChainedBackNavigationTracker::~ChainedBackNavigationTracker() = default;

// static
ChainedBackNavigationTracker* ChainedBackNavigationTracker::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void ChainedBackNavigationTracker::DidStartNavigation(
    content::NavigationHandle* navigation) {
  if (navigation->GetNavigationEntry() &&
      (navigation->GetNavigationEntry()->GetTransitionType() &
       ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK) &&
      !navigation->IsRendererInitiated()) {
    IncrementChainedBackNavigationCount();
  } else {
    ResetChainedBackNavigationCount();
  }
}

void ChainedBackNavigationTracker::IncrementChainedBackNavigationCount() {
  if (chained_back_navigation_count_ == 0 ||
      (base::TimeTicks::Now() - last_back_navigation_time_).InMilliseconds() <=
          kMaxChainedBackNavigationIntervalInMilliseconds) {
    chained_back_navigation_count_++;
  } else {
    chained_back_navigation_count_ = 1u;
  }
  last_back_navigation_time_ = base::TimeTicks::Now();
}

void ChainedBackNavigationTracker::ResetChainedBackNavigationCount() {
  chained_back_navigation_count_ = 0;
  last_back_navigation_time_ = base::TimeTicks();
}

void ChainedBackNavigationTracker::
    RecordBackButtonClickForChainedBackNavigation() {
  if (chained_back_button_click_count_ == 0 ||
      (base::TimeTicks::Now() - last_back_button_click_time_)
              .InMilliseconds() <=
          kMaxChainedBackNavigationIntervalInMilliseconds) {
    chained_back_button_click_count_++;
  } else {
    chained_back_button_click_count_ = 1u;
  }
  last_back_button_click_time_ = base::TimeTicks::Now();
}

bool ChainedBackNavigationTracker::IsChainedBackNavigationRecentlyPerformed()
    const {
  return (base::TimeTicks::Now() - last_back_navigation_time_)
                 .InMilliseconds() <=
             kMaxChainedBackNavigationIntervalInMilliseconds &&
         chained_back_navigation_count_ >= kMinimumChainedBackNavigationLength;
}

bool ChainedBackNavigationTracker::
    IsBackButtonChainedBackNavigationRecentlyPerformed() const {
  return IsChainedBackNavigationRecentlyPerformed() &&
         (base::TimeTicks::Now() - last_back_button_click_time_)
                 .InMilliseconds() <=
             kMaxChainedBackNavigationIntervalInMilliseconds &&
         chained_back_button_click_count_ >=
             kMinimumChainedBackNavigationLength;
}
