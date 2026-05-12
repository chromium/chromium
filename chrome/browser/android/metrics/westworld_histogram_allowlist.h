// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_
#define CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_

#include <tuple>

#include "chrome/browser/android/metrics/uma_utils.h"

namespace chrome::android::westworld {

// An allowlist of UMA histograms that are registered for Westworld logging.
//
// To add a new histogram to Westworld:
// 1. Add the histogram name string here in the list.
// 2. Define an atom to be sent in `westworld_histogram_mapping.cc`.
inline constexpr const char* kWestworldHistogramAllowlist[] = {
    "Tabs.TabCount",
    "Tabs.WindowCount",
};

}  // namespace chrome::android::westworld

#endif  // CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_
