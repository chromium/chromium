// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_JUMPLIST_METRICS_WIN_H_
#define CHROME_BROWSER_METRICS_JUMPLIST_METRICS_WIN_H_

#include <string>

namespace jumplist {

// Enum for counting which category was clicked.
// Note: UMA histogram enum - don't re-order or remove entries
enum JumplistCategory {
  RECENTLY_CLOSED_URL = 0,  // A URL from the "Recently Closed" category.
  MOST_VISITED_URL,         // A URL from the "Most Visited" category.
  SWITCH_TO_PROFILE,        // A profile name from the "People" category.
  CATEGORY_UNKNOWN,         // An invalid category.
  NUM_JUMPLIST_CATEGORY_METRICS
};

// Category types that can be logged with the --win-jumplist-action switch.
extern const char kMostVisitedCategory[];
extern const char kRecentlyClosedCategory[];

// Logs a histogram for the JumplistCategory of the item that was clicked.
void LogJumplistActionFromSwitchValue(const std::string& value);

}  // namespace jumplist

#endif  // CHROME_BROWSER_METRICS_JUMPLIST_METRICS_WIN_H_
