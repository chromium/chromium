// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_

#include "base/feature_list.h"
#include "content/public/browser/preloading.h"

namespace content {
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace prefetch {
BASE_DECLARE_FEATURE(kPreloadingHoldback);

// Enum describing when to allow network predictions.  The numerical value is
// stored in the prefs file, therefore the same enum with the same order must be
// used by the platform-dependent components.
enum class NetworkPredictionOptions {
  kStandard = 0,
  // This option is deprecated. It is now equivalent to kStandard.
  kWifiOnlyDeprecated = 1,
  kDisabled = 2,
  kExtended = 3,
  kDefault = kWifiOnlyDeprecated,
};

// Enum representing possible values of the Preload Pages opt-in state.  Since
// this enum is not persisted in prefs, old values can be removed and new values
// can be added without worry. This is the the sanitized counterpart to
// NetworkPredictionOptions, which is persisted in prefs and cannot be modified
// arbitrarily. Prefer using PreloadPagesState over NetworkPredictionOptions to
// avoid having to deal with deprecated values.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.prefetch.settings
enum class PreloadPagesState {
  // The user is not opted into preloading.
  kNoPreloading = 0,
  // The user selected standard preloading.
  kStandardPreloading = 1,
  // The user selected extended preloading.
  kExtendedPreloading = 2,

  kMaxValue = kExtendedPreloading,
};

// Returns the PreloadPagesState corresponding to the NetworkPredictionOptions
// setting persisted in prefs. Note that this will return the pref value
// regardless of whether preloading is disabled via Finch. Prefer using
// IsSomePreloadingEnabled in most cases.
PreloadPagesState GetPreloadPagesState(const PrefService& prefs);

// Converts the given PreloadPagesState to a NetworkPredictionOptions and
// persist it in prefs.
void SetPreloadPagesState(PrefService* prefs, PreloadPagesState state);

// Returns PreloadingEligibility:kEligible if preloading is not entirely
// disabled. Returns the first blocking reason encountered otherwise.
// TODO(crbug/1391411): Audit the all callsites whether the default_value =
// nullptr is suitable.
content::PreloadingEligibility IsSomePreloadingEnabled(
    const PrefService& prefs,
    content::WebContents* web_contents = nullptr);

// Returns PreloadingEligibility:kEligible if preloading is not entirely
// disabled. Returns the first blocking reason encountered otherwise.
// Ignores the PreloadingHoldback Finch feature.
content::PreloadingEligibility IsSomePreloadingEnabledIgnoringFinch(
    const PrefService& prefs);

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace prefetch

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_
