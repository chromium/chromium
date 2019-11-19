// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/prediction_options.h"

#include "base/logging.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

namespace chrome_browser_net {

namespace {

// Since looking up preferences and current network connection are presumably
// both cheap, we do not cache them here.
NetworkPredictionStatus CanPrefetchAndPrerender(
    int network_prediction_options) {
  switch (network_prediction_options) {
    case NETWORK_PREDICTION_ALWAYS:
    case NETWORK_PREDICTION_WIFI_ONLY:
      if (base::FeatureList::IsEnabled(
              features::kPredictivePrefetchingAllowedOnAllConnectionTypes) ||
          !net::NetworkChangeNotifier::IsConnectionCellular(
              net::NetworkChangeNotifier::GetConnectionType())) {
        return NetworkPredictionStatus::ENABLED;
      }
      return NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK;
    default:
      DCHECK_EQ(NETWORK_PREDICTION_NEVER, network_prediction_options);
      return NetworkPredictionStatus::DISABLED_ALWAYS;
  }
}

bool CanPreresolveAndPreconnect(int network_prediction_options) {
  // DNS preresolution and TCP preconnect are performed even on cellular
  // networks if the user setting is WIFI_ONLY.
  return network_prediction_options != NETWORK_PREDICTION_NEVER;
}

}  // namespace

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      NETWORK_PREDICTION_DEFAULT,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

NetworkPredictionStatus CanPrefetchAndPrerenderUI(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs);
  return CanPrefetchAndPrerender(
      prefs->GetInteger(prefs::kNetworkPredictionOptions));
}

bool CanPreresolveAndPreconnectUI(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs);
  return CanPreresolveAndPreconnect(
      prefs->GetInteger(prefs::kNetworkPredictionOptions));
}

}  // namespace chrome_browser_net
