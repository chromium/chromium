// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_

class PrefService;

namespace prefetch {

// Enum representing possible values of the Preload Pages opt-in state.
// These values are persisted to prefs. Entries should not be renumbered and
// numeric values should never be reused.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.prefetch.settings
enum class PreloadPagesState {
  // The user is not opted into preloading.
  NO_PRELOADING = 0,
  // The user selected standard preloading.
  STANDARD_PRELOADING = 1,
  // The user selected extended preloading.
  EXTENDED_PRELOADING = 2,

  kMaxValue = EXTENDED_PRELOADING,
};

PreloadPagesState GetPreloadPagesState(const PrefService& prefs);

void SetPreloadPagesState(PrefService* prefs, PreloadPagesState state);

}  // namespace prefetch

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PREFS_H_
