// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

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

// If enabled, separate Media Router instances will be created for normal and
// off-the-record profiles.
BASE_DECLARE_FEATURE(kMediaRouterOTRInstance);

#if BUILDFLAG(IS_ANDROID)
// If enabled, and the HTMLMediaElement source changes (e.g. started observing
// new source, and stopped observing the first one), the new source is cast
// instead.
BASE_DECLARE_FEATURE(kCastAnotherContentWhileCasting);
#else

// Enables the media router. Can be disabled in tests unrelated to
// Media Router where it interferes. Can also be useful to disable for local
// development on Mac because DIAL local discovery opens a local port
// and triggers a permission prompt.
BASE_DECLARE_FEATURE(kMediaRouter);

// If enabled, allows Media Router to connect to Cast devices on all IP
// addresses, not just RFC1918/RFC4193 private addresses. Workaround for
// https://crbug.com/813974.
BASE_DECLARE_FEATURE(kCastAllowAllIPsFeature);

// Determine whether global media controls are used to start and stop casting.
BASE_DECLARE_FEATURE(kGlobalMediaControlsCastStartStop);

// If enabled, allows all websites to request to start mirroring via
// Presentation API. If disabled, only the allowlisted sites can do so.
BASE_DECLARE_FEATURE(kAllowAllSitesToInitiateMirroring);

// If enabled, The browser allows discovery of the DIAL support cast device.
// It sends a discovery SSDP message every 120 seconds.
BASE_DECLARE_FEATURE(kDialMediaRouteProvider);

// If enabled, the Cast Media Route Provider starts a session without
// terminating any existing session on the same sink.
BASE_DECLARE_FEATURE(kStartCastSessionWithoutTerminating);

// If enabled, sinks that do not support presentation or remote playback, will
// fall back to audio tab mirroring when casting from the Global Media Controls.
BASE_DECLARE_FEATURE(kFallbackToAudioTabMirroring);

// If enabled, a separate 'stop' button is shown for connected sinks in the Cast
// dialog instead of the entire sink button being a stop button.
BASE_DECLARE_FEATURE(kCastDialogStopButton);

// If enabled, mirroring sessions use the playout delay specified by
// `kCastMirroringPlayoutDelayMs`.
BASE_DECLARE_FEATURE(kCastMirroringPlayoutDelay);
extern const base::FeatureParam<int> kCastMirroringPlayoutDelayMs;

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

// Returns the optional value to use for mirroring playout delay from the
// relevant command line flag or feature, if any are set.
absl::optional<base::TimeDelta> GetCastMirroringPlayoutDelay();
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_FEATURE_H_
