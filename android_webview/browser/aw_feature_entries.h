// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_FEATURE_ENTRIES_H_
#define ANDROID_WEBVIEW_BROWSER_AW_FEATURE_ENTRIES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "components/flags_ui/feature_entry.h"

namespace android_webview {
namespace aw_feature_entries {

// Registers variations parameter values selected for features in WebView.
// The registered variation parameters are connected to their corresponding
// features in |feature_list|. Returns the (possibly empty) comma separated
// list of additional variation ids to register in the MetricsService.
//
// This is a way for WebView to set feature parameters besides the finch,
// for example, add a switch in Dev UI, then setup feature parameters
// according to the switch.
std::vector<std::string> RegisterEnabledFeatureEntries(
    base::FeatureList* feature_list);

// Exposed for testing.
namespace internal {
std::string ToEnabledEntry(const flags_ui::FeatureEntry& entry,
                           int enabled_variation_index);
}  // namespace internal
}  // namespace aw_feature_entries
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_FEATURE_ENTRIES_H_
