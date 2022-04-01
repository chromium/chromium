// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
}

namespace media_router {

// Returns true if Media Router is enabled for |context|.
bool MediaRouterEnabled(content::BrowserContext* context);

// Clears stored prefs so they don't leak between tests running in the same
// process.
void ClearMediaRouterStoredPrefsForTesting();

#if !BUILDFLAG(IS_ANDROID)

// Enables the media router. Can be disabled in tests unrelated to
// Media Router where it interferes. Can also be useful to disable for local
// development on Mac because DIAL local discovery opens a local port
// and triggers a permission prompt.
extern const base::Feature kMediaRouter;

// If enabled, allows Media Router to connect to Cast devices on all IP
// addresses, not just RFC1918/RFC4193 private addresses. Workaround for
// https://crbug.com/813974.
extern const base::Feature kCastAllowAllIPsFeature;

// Determine whether global media controls are used to start and stop casting.
extern const base::Feature kGlobalMediaControlsCastStartStop;

// If enabled, allows all websites to request to start mirroring via
// Presentation API. If disabled, only the allowlisted sites can do so.
extern const base::Feature kAllowAllSitesToInitiateMirroring;

// If enabled, HTTP requests for DIAL can only be made to URLs that contain the
// target device IP address.
// TODO(crbug.com/1270509): Remove this base::Feature once fully launched.
extern const base::Feature kDialEnforceUrlIPAddress;

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

// Returns true if support for DIAL devices is enabled.  Disabling DIAL support
// also disables SSDP-based discovery for Cast devices.
bool DialMediaRouteProviderEnabled();

// Returns true if global media controls are used to start and stop casting and
// Media Router is enabled for |context|.
bool GlobalMediaControlsCastStartStopEnabled(content::BrowserContext* context);

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
