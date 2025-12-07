// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/variant_map.h"

#include <atomic>

#include "base/feature_list.h"

namespace base {

namespace {

// Whether the "AbslFlatMapInVariantMap" feature is enabled.
//
// An atomic is used because this can be queried racily by a thread checking
// which map type to use and another thread initializing this after FeatureList
// initialization. All operations use std::memory_order_relaxed because there
// are no dependent memory operations.
std::atomic_bool g_is_absl_flat_map_in_variant_map_enabled{false};

// Whether absl::flat_hash_map is used by default instead of std::map in
// base::VariantMap.
BASE_FEATURE(kAbslFlatMapInVariantMap, FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

bool IsAbslFlatMapInVariantMapEnabled() {
  return g_is_absl_flat_map_in_variant_map_enabled.load(
      std::memory_order_relaxed);
}

void InitializeVariantMapFeatures() {
  g_is_absl_flat_map_in_variant_map_enabled.store(
      FeatureList::IsEnabled(kAbslFlatMapInVariantMap));
}

}  // namespace base
