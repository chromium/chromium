// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/battery/battery_saver.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/preloading.h"

namespace prefetch {

BASE_FEATURE(kPreloadingHoldback,
             "PreloadingHoldback",
             base::FEATURE_DISABLED_BY_DEFAULT);

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      static_cast<int>(NetworkPredictionOptions::kDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

PreloadPagesState GetPreloadPagesState(const PrefService& prefs) {
  NetworkPredictionOptions network_prediction_options =
      static_cast<NetworkPredictionOptions>(
          prefs.GetInteger(prefs::kNetworkPredictionOptions));
  switch (network_prediction_options) {
    case NetworkPredictionOptions::kExtended:
      return PreloadPagesState::kExtendedPreloading;
    case NetworkPredictionOptions::kStandard:
    case NetworkPredictionOptions::kWifiOnlyDeprecated:
      return PreloadPagesState::kStandardPreloading;
    default:
      // This is what will be used if the enterprise policy sets an invalid
      // value. Also, if a new value is added in the future and the enterprise
      // policy sets this value, old versions of Chrome will use this path.
      return PreloadPagesState::kNoPreloading;
  }
}

void SetPreloadPagesState(PrefService* prefs, PreloadPagesState state) {
  DCHECK(prefs);
  NetworkPredictionOptions value;
  switch (state) {
    case PreloadPagesState::kExtendedPreloading:
      value = NetworkPredictionOptions::kExtended;
      break;
    case PreloadPagesState::kStandardPreloading:
      value = NetworkPredictionOptions::kStandard;
      break;
    case PreloadPagesState::kNoPreloading:
      value = NetworkPredictionOptions::kDisabled;
      break;
    default:
      NOTREACHED() << "invalid PreloadPageState: " << static_cast<int>(state);
      return;
  }
  prefs->SetInteger(prefs::kNetworkPredictionOptions, static_cast<int>(value));
}

content::PreloadingEligibility IsSomePreloadingEnabled(
    const PrefService& prefs,
    content::WebContents* web_contents) {
  // Override kPreloadingHoldback when DevTools is opened to mitigate the cases
  // in which developers are affected by kPreloadingHoldback.
  if (!(web_contents && content::DevToolsAgentHost::HasFor(web_contents)) &&
      base::FeatureList::IsEnabled(kPreloadingHoldback)) {
    return content::PreloadingEligibility::kPreloadingDisabled;
  }
  return IsSomePreloadingEnabledIgnoringFinch(prefs);
}

content::PreloadingEligibility IsSomePreloadingEnabledIgnoringFinch(
    const PrefService& prefs) {
  if (battery::IsBatterySaverEnabled()) {
    return content::PreloadingEligibility::kBatterySaverEnabled;
  }
  if (GetPreloadPagesState(prefs) == PreloadPagesState::kNoPreloading) {
    return content::PreloadingEligibility::kPreloadingDisabled;
  }

  return content::PreloadingEligibility::kEligible;
}

}  // namespace prefetch
