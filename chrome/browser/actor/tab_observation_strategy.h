// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TAB_OBSERVATION_STRATEGY_H_
#define CHROME_BROWSER_ACTOR_TAB_OBSERVATION_STRATEGY_H_

#include "base/containers/flat_map.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

// Options that can be used to influence whether a tab screenshot is taken
// after a set of actions has completed. These votes are combined to help
// determine the final decision about screenshots.
//
// The priority of policies increases with their integer value (kRequired >
// kRequested > kSkipped), allowing them to be combined using std::max to find
// the highest priority vote.
enum class ScreenshotPolicy {
  // A screenshot should be skipped. Even if performed, the result will be
  // unused.
  kSkipped = 0,

  // A screenshot should be taken, but it will not cause a task failure if it is
  // skipped.
  kRequested = 1,

  // A screenshot must be taken, and a task failure will occur if it is not
  // provided.
  kRequired = 2,
};

// Options a client can use to influence whether a page content extraction
// (i.e. the construction of `AiPageContent`) is done after a set of actions has
// completed. These votes are combined to help determine the final decision
// about page content extraction.
//
// The priority of policies increases with their integer value (kRequired >
// kRequested > kSkipped), allowing them to be combined using std::max to find
// the highest priority vote.
enum class PageContentExtractionPolicy {
  // Page content extraction should be skipped. Even if performed, the result
  // will be unused.
  kSkipped = 0,

  // Page content extraction should be done, but it will not cause a task
  // failure
  // if it is skipped.
  kRequested = 1,

  // Page content extraction must be done, and a task failure will occur if it
  // is
  // not.
  kRequired = 2,
};

// Collects observation votes for one or more tabs and tallies them to determine
// whether screenshots and/or page content extraction is performed after actions
// are complete. Incremental vote tallying is performed at the time of voting
// using std::max. Once actions are complete, the strategy should be "locked" so
// that the final decisions remain immutable.
class TabObservationStrategy {
 public:
  TabObservationStrategy();
  TabObservationStrategy(const TabObservationStrategy&) = delete;
  TabObservationStrategy& operator=(const TabObservationStrategy&) = delete;
  TabObservationStrategy(TabObservationStrategy&&);
  TabObservationStrategy& operator=(TabObservationStrategy&&);
  ~TabObservationStrategy();

  // Registers a vote for a specific tab. Tools should vote once per tab.
  void VoteForScreenshot(tabs::TabHandle tab, ScreenshotPolicy policy);
  void VoteForPageContentExtraction(tabs::TabHandle tab,
                                    PageContentExtractionPolicy policy);

  // Locks the object and prevents further voting.
  void Lock();

  // Retrieves the final outcome policies for a tab.
  ScreenshotPolicy GetScreenshotPolicy(tabs::TabHandle tab) const;
  PageContentExtractionPolicy GetPageContentExtractionPolicy(
      tabs::TabHandle tab) const;

 private:
  base::flat_map<tabs::TabHandle, ScreenshotPolicy> screenshot_votes_;
  base::flat_map<tabs::TabHandle, PageContentExtractionPolicy>
      extraction_votes_;
  bool locked_ = false;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TAB_OBSERVATION_STRATEGY_H_
