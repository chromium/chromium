// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_features.h"
#include "base/feature_list.h"

namespace base::android::features {

// Alphabetical:

// When the browser process has been in the background for several minutes at a
// time, trigger an artificial critical memory pressure notification. This is
// intended to reduce memory footprint.
BASE_FEATURE(kBrowserProcessMemoryPurge,
             "BrowserProcessMemoryPurge",
             FEATURE_ENABLED_BY_DEFAULT);

// Crash the browser process if a child process is created which does not match
// the browser process and the browser package appears to have changed since the
// browser process was launched, so that the browser process will be started
// fresh when next used, hopefully resolving the issue.
BASE_FEATURE(kCrashBrowserOnChildMismatchIfBrowserChanged,
             "CrashBrowserOnChildMismatchIfBrowserChanged",
             FEATURE_DISABLED_BY_DEFAULT);

// Crash the browser process if a child process is created which does not match
// the browser process regardless of whether the browser package appears to have
// changed.
BASE_FEATURE(kCrashBrowserOnAnyChildMismatch,
             "CrashBrowserOnAnyChildMismatch",
             FEATURE_DISABLED_BY_DEFAULT);

}  // namespace base::android::features
