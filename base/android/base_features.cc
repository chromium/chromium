// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_features.h"
#include "base/feature_list.h"

namespace base::android::features {

// Alphabetical:

// Crash the browser process if a child process is created which does not match
// the browser process and the browser package appears to have changed since the
// browser process was launched, so that the browser process will be started
// fresh when next used, hopefully resolving the issue.
const base::Feature kCrashBrowserOnChildMismatchIfBrowserChanged{
    "CrashBrowserOnChildMismatchIfBrowserChanged", FEATURE_DISABLED_BY_DEFAULT};

// Crash the browser process if a child process is created which does not match
// the browser process regardless of whether the browser package appears to have
// changed.
const base::Feature kCrashBrowserOnAnyChildMismatch{
    "CrashBrowserOnAnyChildMismatch", FEATURE_DISABLED_BY_DEFAULT};

}  // namespace base::android::features
