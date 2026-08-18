// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_
#define CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_

#include "base/functional/callback_forward.h"
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
// Defined in per-platform files (e.g. platform_util_win.cc).
void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type);

// Prevent shell or external applications from being invoked during testing.
void DisableShellOperationsForTesting();

// Returns false if DisableShellOperationsForTesting() has been called.
bool AreShellOperationsAllowed();

// If set, invoked on the worker thread OpenItem() schedules its
// verify-and-open work on, before that work runs (regardless of whether
// DisableShellOperationsForTesting() was called). Lets tests observe
// properties of that thread, e.g. its COM apartment type on Windows.
void SetOpenItemThreadObserverForTesting(base::RepeatingClosure observer);

// Runs the observer set by SetOpenItemThreadObserverForTesting(), if any.
void RunOpenItemThreadObserverForTesting();

}  // namespace internal
}  // namespace platform_util

#endif  // CHROME_BROWSER_PLATFORM_UTIL_INTERNAL_H_
