// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_entries.h"

#include <set>

#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/blink/public/common/features.h"

namespace android_webview {

namespace aw_feature_entries {
namespace {

constexpr flags_ui::FeatureEntry::FeatureParam
    kForceDark_SelectiveImageInversion[] = {
        {"inversion_method", "cielab_based"},
        {"image_behavior", "selective"},
        {"foreground_lightness_threshold", "150"},
        {"background_lightness_threshold", "205"}};

// Not like Chrome, WebView only provides a switch in dev ui and uses the
// preferred variation if it is turned on.
constexpr flags_ui::FeatureEntry::FeatureVariation kForceDarkVariations[] = {
    {"with selective image inversion", kForceDark_SelectiveImageInversion,
     std::size(kForceDark_SelectiveImageInversion), nullptr}};

// Not for display, set the descriptions to empty.
constexpr flags_ui::FeatureEntry kForceDark = {
    "enable-force-dark", "", "", flags_ui::kOsWebView,
    FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                   kForceDarkVariations,
                                   "ForceDarkVariations")};

constexpr flags_ui::FeatureEntry kWebViewFeatureEntries[] = {
    kForceDark,
};

}  // namespace

namespace internal {
std::string ToEnabledEntry(const flags_ui::FeatureEntry& entry,
                           int enabled_variation_index) {
  CHECK(entry.type == flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  // Index 0 is "Default" and 1 is "Enabled" inside FeatureEntry.
  return entry.NameForOption(enabled_variation_index + 2);
}
}  // namespace internal

std::vector<std::string> RegisterEnabledFeatureEntries(
    base::FeatureList* feature_list) {
  std::set<std::string> enabled_entries;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewSelectiveImageInversionDarkening)) {
    enabled_entries.insert(internal::ToEnabledEntry(kForceDark, 0));
  }
  return flags_ui::FlagsState::RegisterEnabledFeatureVariationParameters(
      kWebViewFeatureEntries, enabled_entries, /*trial_name=*/"webview_dev_ui",
      feature_list);
}

}  // namespace aw_feature_entries

}  // namespace android_webview
