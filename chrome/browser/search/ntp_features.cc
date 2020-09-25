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

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
const base::Feature kConfirmSuggestionRemovals{
    "ConfirmNtpSuggestionRemovals", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissPromos{"DismissNtpPromos",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the OneGooleBar is loaded in an iframe. Otherwise, it is inlined.
const base::Feature kIframeOneGoogleBar{"IframeOneGoogleBar",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, queries that are frequently repeated by the user (and are
// expected to be issued again) are shown as most visited tiles.
const base::Feature kNtpRepeatableQueries{"NtpRepeatableQueries",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the iframed OneGooleBar shows the overlays modally with a
// backdrop.
const base::Feature kOneGoogleBarModalOverlays{
    "OneGoogleBarModalOverlays", base::FEATURE_DISABLED_BY_DEFAULT};

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
const base::Feature kRealbox{"NtpRealbox", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, shows Vasco suggestion chips in the NTP below fakebox/realbox
// despite other config except DisableSearchSuggestChips below.
const base::Feature kSearchSuggestChips{"SearchSuggestChips",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, hides Vasco suggestion chips in the NTP below fakebox/realbox
// despite other config.
const base::Feature kDisableSearchSuggestChips{
    "DisableSearchSuggestChips", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the WebUI new tab page will load when a new tab is created
// instead of the local NTP.
const base::Feature kWebUI{"NtpWebUI", base::FEATURE_DISABLED_BY_DEFAULT};

// If disabled, the realbox will not show on the WebUI new tab page.
const base::Feature kWebUIRealbox{"WebUIRealbox",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the Doodle will be shown on themed and dark mode NTPs.
const base::Feature kWebUIThemeModeDoodles{"WebUIThemeModeDoodles",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, modules will be shown.
const base::Feature kModules{"NtpModules", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shopping tasks module will be shown.
const base::Feature kNtpShoppingTasksModule{"NtpShoppingTasksModule",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsRealboxEnabled() {
  if (!base::FeatureList::IsEnabled(omnibox::kNewSearchFeatures))
    return false;

  return base::FeatureList::IsEnabled(kRealbox) ||
         base::FeatureList::IsEnabled(omnibox::kZeroSuggestionsOnNTPRealbox) ||
         base::FeatureList::IsEnabled(
             omnibox::kReactiveZeroSuggestionsOnNTPRealbox) ||
         (base::FeatureList::IsEnabled(omnibox::kOnFocusSuggestions) &&
          !OmniboxFieldTrial::GetZeroSuggestVariants(
               metrics::OmniboxEventProto::NTP_REALBOX)
               .empty());
}

}  // namespace ntp_features
