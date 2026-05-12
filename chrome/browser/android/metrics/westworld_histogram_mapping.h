// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_MAPPING_H_
#define CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_MAPPING_H_

#include <optional>
#include <string_view>

namespace chrome::android::westworld {

// Defines the expected data type for the Westworld atom.
// Westworld atoms expect specific types (e.g., integers or booleans). This enum
// ensures the correct JNI method is called when logging to Westworld.
enum class MetricType { kInt, kBoolean };

// Maps a UMA histogram name to its corresponding Westworld Atom ID and type.
// This is required because UMA identifies metrics by string names, whereas
// Westworld (the Android logging system) identifies them by integer Atom IDs.
struct MappingInfo {
  int atom_id;
  MetricType type;
};

// Returns the mapping info for a given UMA histogram name.
// Returns std::nullopt if the histogram is not mapped.
std::optional<MappingInfo> GetAtomMappingInfo(std::string_view histogram_name);

}  // namespace chrome::android::westworld

#endif  // CHROME_BROWSER_ANDROID_METRICS_WESTWORLD_HISTOGRAM_MAPPING_H_
