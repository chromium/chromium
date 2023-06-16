// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "crypto/random.h"
#include "media/base/media_switches.h"
#include "ui/base/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_registry_simple.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_types_ash.h"
#endif

namespace media_router {

BASE_FEATURE(kMediaRouterOTRInstance,
             "MediaRouterOTRInstance",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCafMRPDeferredDiscovery,
             "CafMRPDeferredDiscovery",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCastAnotherContentWhileCasting,
             "CastAnotherContentWhileCasting",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
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
BASE_FEATURE(kStartCastSessionWithoutTerminating,
             "StartCastSessionWithoutTerminating",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFallbackToAudioTabMirroring,
             "FallbackToAudioTabMirroring",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCastDialogStopButton,
             "CastDialogStopButton",
             base::FEATURE_ENABLED_BY_DEFAULT);
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kGlobalMediaControlsCastStartStop,
             "GlobalMediaControlsCastStartStop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGlobalMediaControlsCastStartStop,
             "GlobalMediaControlsCastStartStop",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_ANDROID)

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
  if (!IsUserProfile(Profile::FromBrowserContext(context)))
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!base::FeatureList::IsEnabled(kMediaRouterOTRInstance)) {
    // The MediaRouter service is shared across the original and the incognito
    // profiles, so we must use the original context for consistency between
    // them.
    context = chrome::GetBrowserContextRedirectedInIncognito(context);
  }

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
    crypto::RandBytes(base::WriteInto(&token, kHashTokenSize + 1),
                      kHashTokenSize);
    base::Base64Encode(token, &token);
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

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
