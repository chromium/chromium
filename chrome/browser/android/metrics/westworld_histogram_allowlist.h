// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_
#define CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_

#include <string_view>

#include "chrome/browser/android/metrics/uma_utils.h"

namespace chrome::android::westworld {

// Defines the expected data type for the Westworld atom.
// Westworld atoms expect specific types (e.g., integers or booleans). This enum
// ensures the correct JNI method is called when logging to Westworld.
enum class MetricType { kInt, kBoolean };

// Holds information about a UMA histogram that is mapped to a Westworld atom.
struct HistogramInfo {
  // The name of the UMA histogram to observe.
  std::string_view histogram_name;
  // The corresponding Westworld Atom ID.
  int ww_atom_id;
  // The data type expected by the Westworld atom.
  MetricType type;
};

// An allowlist of UMA histograms that are registered for Westworld logging.
//
// To add a new histogram to Westworld:
// Add a new HistogramInfo entry to this array with the UMA histogram name,
// the corresponding Westworld Atom ID, and the expected MetricType.
inline constexpr const HistogramInfo kWestworldHistogramAllowlist[] = {
    {"Tabs.TabCount", 215200, MetricType::kInt},
    {"Tabs.WindowCount", 215201, MetricType::kInt},
};

}  // namespace chrome::android::westworld

#endif  // CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_ALLOWLIST_H_
