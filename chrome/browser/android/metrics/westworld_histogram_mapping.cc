// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/westworld_histogram_mapping.h"

#include <iterator>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/android/metrics/westworld_histogram_allowlist.h"

namespace chrome::android::westworld {

namespace {

// A mapping of UMA histograms to their corresponding Westworld Atom IDs.
constexpr auto kWestworldHistogramMapping =
    base::MakeFixedFlatMap<std::string_view, MappingInfo>({
        {"Tabs.TabCount", {215200, MetricType::kInt}},
        {"Tabs.WindowCount", {215201, MetricType::kInt}},
    });

// Helper to check if a compile-time map contains a key.
// We cannot use `map.contains(key)` here because
// `base::fixed_flat_map::contains` is not marked `constexpr`.
// TODO: crbug.com/511193292 - Change to use `base::fixed_flat_map::contains`
// after it's marked as constexpr.template <typename Map>
template <typename Map>
constexpr bool CompileTimeMapContains(const Map& map, std::string_view key) {
  for (const auto& pair : map) {
    if (pair.first == key) {
      return true;
    }
  }
  return false;
}

// A compile-time helper to validate that every histogram in the allowlist
// has a corresponding entry in the mapping array.
template <size_t N, typename Map>
constexpr bool ValidateAllowlist(const char* const (&allowlist)[N],
                                 const Map& map) {
  for (const char* name : allowlist) {
    if (!CompileTimeMapContains(map, std::string_view(name))) {
      return false;
    }
  }
  return true;
}

static_assert(ValidateAllowlist(kWestworldHistogramAllowlist,
                                kWestworldHistogramMapping),
              "A histogram in the allowlist is missing its mapping details! "
              "You may want to update `kWestworldHistogramMapping`.");

static_assert(!kWestworldHistogramMapping.empty(),
              "kWestworldHistogramMapping should not be empty");

}  // namespace

std::optional<MappingInfo> GetAtomMappingInfo(std::string_view histogram_name) {
  auto it = kWestworldHistogramMapping.find(histogram_name);
  if (it != kWestworldHistogramMapping.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace chrome::android::westworld
