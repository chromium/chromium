// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_
#define CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace chrome_browser_net {

// Enum describing when to allow network predictions based on connection type.
// The numerical value is stored in the prefs file, therefore the same enum
// with the same order must be used by the platform-dependent components.
enum NetworkPredictionOptions {
  // TODO(newt): collapse ALWAYS and WIFI_ONLY into a single value. See
  // crbug.com/585297
  NETWORK_PREDICTION_ALWAYS,
  NETWORK_PREDICTION_WIFI_ONLY,
  NETWORK_PREDICTION_NEVER,
  NETWORK_PREDICTION_DEFAULT = NETWORK_PREDICTION_WIFI_ONLY,
};

enum class NetworkPredictionStatus {
  ENABLED,
  DISABLED_ALWAYS,
  DISABLED_DUE_TO_NETWORK,
};

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Determines whether prefetching and prerendering are enabled, based on
// preferences and network type.
NetworkPredictionStatus CanPrefetchAndPrerenderUI(PrefService* prefs);

// Determines whether TCP preconnect and DNS preresolution are enabled, based on
// preferences.
bool CanPreresolveAndPreconnectUI(PrefService* prefs);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_
