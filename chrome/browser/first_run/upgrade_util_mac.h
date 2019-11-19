// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_MAC_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_MAC_H_

namespace upgrade_util {

struct ThisAndOtherUserCounts {
  int this_user_count;
  int other_user_count;
};

// Returns the number of other running instances of this binary, both by this
// user and by other users. Returns 0 for the counts if there is an error.
ThisAndOtherUserCounts GetCountOfOtherInstancesOfThisBinary();

// Returns whether it is safe to relaunch Chromium to take a staged upgrade.
// Returns true if there are no other instances of Chromium running; returns
// false if there are other instances and they cannot be stopped.
bool ShouldContinueToRelaunchForUpgrade();

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_MAC_H_
