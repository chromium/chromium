// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_

#include "base/feature_list.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
}

namespace media_router {

// Returns true if Media Router is enabled for |context|.
bool MediaRouterEnabled(content::BrowserContext* context);

#if !defined(OS_ANDROID)

extern const base::Feature kDialMediaRouteProvider;
// TODO(crbug.com/969091): This feature is now enabled by default, and the flag
// should be removed.
extern const base::Feature kEnableCastDiscovery;
extern const base::Feature kCastMediaRouteProvider;
// If enabled, allows Media Router to connect to Cast devices on all IP
// addresses, not just RFC1918/RFC4193 private addresses. Workaround for
// https://crbug.com/813974.
extern const base::Feature kCastAllowAllIPsFeature;

namespace prefs {
// Pref name for the enterprise policy for allowing Cast devices on all IPs.
constexpr char kMediaRouterCastAllowAllIPs[] =
    "media_router.cast_allow_all_ips";
// Pref name for the per-profile randomly generated token to include with the
// hash when externalizing MediaSink IDs.
constexpr char kMediaRouterReceiverIdHashToken[] =
    "media_router.receiver_id_hash_token";
}  // namespace prefs

// Registers |kMediaRouterCastAllowAllIPs| with local state pref |registry|.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Registers Media Router related preferences with per-profile pref |registry|.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if CastMediaSinkService can connect to Cast devices on
// all IPs, as determined by local state |pref_service| / feature flag.
bool GetCastAllowAllIPsPref(PrefService* pref_service);

// Returns the hash token to use for externalizing MediaSink IDs from
// |pref_service|. If the token does not exist, the token will be created from a
// randomly generated string and stored in |pref_service|.
std::string GetReceiverIdHashToken(PrefService* pref_service);

// Returns true if browser side DIAL Media Route Provider is enabled.
bool DialMediaRouteProviderEnabled();

// Returns true if browser side Cast discovery is enabled.
bool CastDiscoveryEnabled();

// Returns true if browser side Cast Media Route Provider and sink query are
// enabled.
bool CastMediaRouteProviderEnabled();

// Returns true if the Views implementation of the Cast dialog should be used.
// Returns false if the WebUI implementation should be used.
// TODO(crbug.com/969098): The feature is now enabled by default. Remove this
// function.
bool ShouldUseViewsDialog();

// Returns true if Mirroring Service should be used for mirroring.
bool ShouldUseMirroringService();

#endif  // !defined(OS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
