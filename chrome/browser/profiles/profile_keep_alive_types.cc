// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keep_alive_types.h"

#include "base/notreached.h"

std::ostream& operator<<(std::ostream& out,
                         const ProfileKeepAliveOrigin& origin) {
  switch (origin) {
    case ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow:
      return out << "kWaitingForFirstBrowserWindow";
    case ProfileKeepAliveOrigin::kBrowserWindow:
      return out << "kBrowserWindow";
    case ProfileKeepAliveOrigin::kBackgroundMode:
      return out << "kBackgroundMode";
    case ProfileKeepAliveOrigin::kOffTheRecordProfile:
      return out << "kOffTheRecordProfile";
  }
  NOTREACHED();
  return out << static_cast<int>(origin);
}
