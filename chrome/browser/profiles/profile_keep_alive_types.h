// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_

#include <ostream>

// Refers to what a ScopedProfileKeepAlive's lifetime is tied to, to help
// debugging.
enum class ProfileKeepAliveOrigin {
  // When a Profile gets created by ProfileManager, it initially has this type
  // of keep-alive. This ensures that the Profile has a refcount >=1, at least
  // until RemoveKeepAlive() gets called.
  //
  // When a kBrowserWindow keep-alive gets added, this one gets removed.
  kWaitingForFirstBrowserWindow,

  // This Profile has browser windows open.
  kBrowserWindow,

  // This Profile is running extensions with persistent background scripts.
  kBackgroundMode,

  // A child off-the-record profile holds a strong reference to its parent.
  kOffTheRecordProfile,
};

std::ostream& operator<<(std::ostream& out,
                         const ProfileKeepAliveOrigin& origin);

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEEP_ALIVE_TYPES_H_
