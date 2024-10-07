// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "crypto/random.h"
#include "media/base/media_switches.h"
#include "ui/base/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

// NOTE: Consider separating out UI-only features that are not consumed by the
// Media Router itself into their own file in chrome/browser/ui/media_router.

namespace media_router {

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMediaRouter, "MediaRouter", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCastAllowAllIPsFeature,
             "CastAllowAllIPs",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAllowAllSitesToInitiateMirroring,
             "AllowAllSitesToInitiateMirroring",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDialMediaRouteProvider,
             "DialMediaRouteProvider",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDelayMediaSinkDiscovery,
             "DelayMediaSinkDiscovery",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kShowCastPermissionRejectedError,
             "ShowCastPermissionRejectedError",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1486680): Remove once stopping mirroring routes in the global
// media controls is implemented on ChromeOS.
BASE_FEATURE(kFallbackToAudioTabMirroring,
             "FallbackToAudioTabMirroring",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// TODO(b/202294946): Remove when enabled by default after a few milestones.
BASE_FEATURE(kGlobalMediaControlsCastStartStop,
             "GlobalMediaControlsCastStartStop",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCastSilentlyRemoveVcOnNavigation,
             "CastSilentlyRemoveVcOnNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kUseNetworkFrameworkForLocalDiscovery,
             "UseNetworkFrameworkForLocalDiscovery",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace {
const PrefService::Preference* GetMediaRouterPref(
    content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context)->FindPreference(
      ::prefs::kEnableMediaRouter);
}

base::flat_map<content::BrowserContext*, bool>& GetStoredPrefValues() {
  static base::NoDestructor<base::flat_map<content::BrowserContext*, bool>>
      stored_pref_values;

  return *stored_pref_values;
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(mfoltz): Add full implementation for validating playout delay value.
bool IsValidMirroringPlayoutDelayMs(int delay_ms) {
  return delay_ms <= 1000 && delay_ms >= 1;
}
#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace

void ClearMediaRouterStoredPrefsForTesting() {
  GetStoredPrefValues().clear();
}

bool MediaRouterEnabled(content::BrowserContext* context) {
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kMediaRouter))
    return false;
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1380828): Make the Media Router feature configurable via a
  // policy for non-user profiles, i.e. sign-in and lock screen profiles.
  if (!ash::IsUserBrowserContext(context)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If the Media Router was already enabled or disabled for |context|, then it
  // must remain so.  The Media Router does not support dynamic
  // enabling/disabling.
  base::flat_map<content::BrowserContext*, bool>& pref_values =
      GetStoredPrefValues();
  auto const it = pref_values.find(context);
  if (it != pref_values.end())
    return it->second;

  // Check the enterprise policy.
  const PrefService::Preference* pref = GetMediaRouterPref(context);
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    CHECK(pref->GetValue()->is_bool());
    bool allowed = pref->GetValue()->GetBool();
    pref_values.insert(std::make_pair(context, allowed));
    return allowed;
  }
  return true;
}

#if !BUILDFLAG(IS_ANDROID)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMediaRouterCastAllowAllIPs, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kSuppressLocalDiscoveryPermissionError,
                                false);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMediaRouterReceiverIdHashToken, "",
                               PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      true);
}

bool GetCastAllowAllIPsPref(PrefService* pref_service) {
  auto* pref = pref_service->FindPreference(prefs::kMediaRouterCastAllowAllIPs);

  // Only use the pref value if it is set from a mandatory policy.
  bool allow_all_ips = false;
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    CHECK(pref->GetValue()->is_bool());
    allow_all_ips = pref->GetValue()->GetBool();
  } else {
    allow_all_ips = base::FeatureList::IsEnabled(kCastAllowAllIPsFeature);
  }

  return allow_all_ips;
}

std::string GetReceiverIdHashToken(PrefService* pref_service) {
  static constexpr size_t kHashTokenSize = 64;
  std::string token =
      pref_service->GetString(prefs::kMediaRouterReceiverIdHashToken);
  if (token.empty()) {
    std::array<uint8_t, kHashTokenSize> rand_token;
    crypto::RandBytes(rand_token);
    token = base::Base64Encode(rand_token);
    pref_service->SetString(prefs::kMediaRouterReceiverIdHashToken, token);
  }
  return token;
}

bool DialMediaRouteProviderEnabled() {
  return base::FeatureList::IsEnabled(kDialMediaRouteProvider);
}

bool GlobalMediaControlsCastStartStopEnabled(content::BrowserContext* context) {
  return base::FeatureList::IsEnabled(kGlobalMediaControlsCastStartStop) &&
         MediaRouterEnabled(context);
}

std::optional<base::TimeDelta> GetCastMirroringPlayoutDelay() {
  std::optional<base::TimeDelta> target_playout_delay;

  // The default playout delay can be overridden with the command line flag
  // `cast-mirroring-target-playout-delay`.
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kCastMirroringTargetPlayoutDelay)) {
    int switch_playout_delay = 0;
    if (base::StringToInt(
            cl->GetSwitchValueASCII(switches::kCastMirroringTargetPlayoutDelay),
            &switch_playout_delay) &&
        IsValidMirroringPlayoutDelayMs(switch_playout_delay)) {
      target_playout_delay = base::Milliseconds(switch_playout_delay);
    }
  }

  return target_playout_delay;
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
