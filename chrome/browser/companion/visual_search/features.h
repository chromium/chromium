// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_FEATURES_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace visual_search {
namespace features {

// Enables visual search capabilities for the companion.
BASE_DECLARE_FEATURE(kVisualSearchSuggestions);

}  // namespace features

namespace switches {
extern const char kVisualSearchConfigForCompanion[];

absl::optional<std::string> GetVisualSearchConfigForCompanionOverride();

}  // namespace switches
}  // namespace visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_FEATURES_H_
