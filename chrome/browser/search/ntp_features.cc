// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/ui_base_features.h"

namespace ntp_features {

// If enabled, 'Chrome Colors' menu becomes visible in the customization picker.
const base::Feature kChromeColors{"ChromeColors",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, custom color picker becomes visible in 'Chrome Colors' menu.
const base::Feature kChromeColorsCustomColorPicker{
    "ChromeColorsCustomColorPicker", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
const base::Feature kConfirmSuggestionRemovals{
    "ConfirmNtpSuggestionRemovals", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see the second version of the customization picker.
const base::Feature kCustomizationMenuV2{"NtpCustomizationMenuV2",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissPromos{"DismissNtpPromos",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Depends on kRealbox being enabled. If enabled, the NTP "realbox" will be
// themed like the omnibox (same background/text/selected/hover colors).
const base::Feature kRealboxMatchOmniboxTheme{
    "NtpRealboxMatchOmniboxTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
const base::Feature kRealboxUseGoogleGIcon{"NtpRealboxUseGoogleGIcon",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the search box in the middle of the NTP will accept input
// directly (i.e. not be a "fake" box) and search results will show directly
// below the non-fake input ("realbox").
const base::Feature kRealbox{"NtpRealbox", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsRealboxEnabled() {
  return base::FeatureList::IsEnabled(kRealbox) ||
         base::FeatureList::IsEnabled(omnibox::kZeroSuggestionsOnNTPRealbox) ||
         (base::FeatureList::IsEnabled(omnibox::kOnFocusSuggestions) &&
          !OmniboxFieldTrial::GetZeroSuggestVariants(
               metrics::OmniboxEventProto::NTP_REALBOX)
               .empty());
}

}  // namespace ntp_features
