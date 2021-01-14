// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_

#include <ostream>

#include "build/build_config.h"

// Refers to what a ScopedProfileKeepAlive's lifetime is tied to, to help
// debugging.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Keep this in sync with ProfileKeepAliveOrigin in enums.xml.
enum class ProfileKeepAliveOrigin {
  // When a Profile gets created by ProfileManager, it initially has this type
  // of keep-alive. This ensures that the Profile has a refcount >=1, at least
  // until RemoveKeepAlive() gets called.
  //
  // When a kBrowserWindow keep-alive gets added, this one gets removed.
  kWaitingForFirstBrowserWindow = 0,

  // This Profile has browser windows open.
  kBrowserWindow = 1,

  // This Profile is running extensions with persistent background scripts.
  kBackgroundMode = 2,

  // A child off-the-record profile holds a strong reference to its parent.
  kOffTheRecordProfile = 3,

  // This Profile is downloading a file.
  kDownloadInProgress = 4,

  // On macOS, Chrome doesn't exit when all windows are closed. Keep one Profile
  // alive so we can open windows for the last-used Profile when the user
  // "launches" Chrome again.
  kAppControllerMac = 5,

  // In the middle of clearing browsing data during browsing exit, for the
  // ClearBrowsingDataOnExistList policy.
  kClearingBrowsingData = 6,

  kMaxValue = kClearingBrowsingData,
};

std::ostream& operator<<(std::ostream& out,
                         const ProfileKeepAliveOrigin& origin);

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_
