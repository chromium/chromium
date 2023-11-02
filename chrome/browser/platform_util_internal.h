// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_
#define CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_

#include "chrome/browser/platform_util.h"

namespace base {
class FilePath;
}

namespace platform_util {
namespace internal {

// Called by platform_util.cc on desktop platforms to invoke platform specific
// logic to open |path| using a suitable handler. |path| has been verified to be
// of type |type|. Called on the thread pool with
// base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN semantics (and thus can't
// use global state torn down during shutdown).
void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type);

// Prevent shell or external applications from being invoked during testing.
void DisableShellOperationsForTesting();

// Returns false if DisableShellOperationsForTesting() has been called.
bool AreShellOperationsAllowed();

}  // namespace internal
}  // namespace platform_util

#endif  // CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_
