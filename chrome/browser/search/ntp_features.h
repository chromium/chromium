// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kChromeColors;
extern const base::Feature kChromeColorsCustomColorPicker;
extern const base::Feature kConfirmSuggestionRemovals;
extern const base::Feature kCustomizationMenuV2;
extern const base::Feature kDismissPromos;
extern const base::Feature kRealboxMatchOmniboxTheme;
extern const base::Feature kRealboxUseGoogleGIcon;

// Note: only exposed for about:flags. Use IsNtpRealboxEnabled() instead.
extern const base::Feature kRealbox;

// Returns true if either kRealbox or omnibox::kZeroSuggestionsOnNTPRealbox
// are enabled; or omnibox::kOnFocusSuggestions is enabled and configured to
// show suggestions of some type in the NTP Realbox.
bool IsRealboxEnabled();

}  // namespace ntp_features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
