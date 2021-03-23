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

// Enables the media router. Can be disabled in tests unrelated to
// Media Router where it interferes. Can also be useful to disable for local
// development on Mac because DIAL local discovery opens a local port
// and triggers a permission prompt.
extern const base::Feature kMediaRouter;

// TODO(crbug.com/1028753): Remove default-enabled kDialMediaRouteProvider after
// tests stop disabling it.
extern const base::Feature kDialMediaRouteProvider;

extern const base::Feature kCastMediaRouteProvider;

// If enabled, allows Media Router to connect to Cast devices on all IP
// addresses, not just RFC1918/RFC4193 private addresses. Workaround for
// https://crbug.com/813974.
extern const base::Feature kCastAllowAllIPsFeature;

// Determine whether global media controls are used to start and stop casting.
extern const base::Feature kGlobalMediaControlsCastStartStop;

// If enabled, allows all websites to request to start mirroring via
// Presentation API. If disabled, only the allowlisted sites can do so.
extern const base::Feature kAllowAllSitesToInitiateMirroring;

// If enabled, meetings appear as receivers in the Cast menu.
extern const base::Feature kCastToMeetingFromCastDialog;

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

// Returns true if browser side Cast Media Route Provider and sink query are
// enabled.
bool CastMediaRouteProviderEnabled();

// Returns true if global media controls are used to start and stop casting.
bool GlobalMediaControlsCastStartStopEnabled();
#endif  // !defined(OS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
