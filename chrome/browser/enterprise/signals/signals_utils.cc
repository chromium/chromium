// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_utils.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(IS_WIN)
#include "components/component_updater/pref_names.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_signals {
namespace utils {

namespace {

bool IsURLBlocked(const GURL& url, PolicyBlocklistService* service) {
  if (!service)
    return false;

  policy::URLBlocklist::URLBlocklistState state =
      service->GetURLBlocklistState(url);

  return state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

}  // namespace

safe_browsing::SafeBrowsingState GetSafeBrowsingProtectionLevel(
    PrefService* profile_prefs) {
  DCHECK(profile_prefs);
  bool safe_browsing_enabled =
      profile_prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile_prefs->GetBoolean(prefs::kSafeBrowsingEnhanced);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled)
      return safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION;
    else
      return safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
  } else {
    return safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
  }
}

std::optional<bool> GetThirdPartyBlockingEnabled(PrefService* local_state) {
  DCHECK(local_state);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return local_state->GetBoolean(prefs::kThirdPartyBlockingEnabled);
#else
  return std::nullopt;
#endif
}

bool GetBuiltInDnsClientEnabled(PrefService* local_state) {
  DCHECK(local_state);
  return local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

std::optional<safe_browsing::PasswordProtectionTrigger>
GetPasswordProtectionWarningTrigger(PrefService* profile_prefs) {
  DCHECK(profile_prefs);
  if (!profile_prefs->HasPrefPath(prefs::kPasswordProtectionWarningTrigger))
    return std::nullopt;
  return static_cast<safe_browsing::PasswordProtectionTrigger>(
      profile_prefs->GetInteger(prefs::kPasswordProtectionWarningTrigger));
}

bool GetChromeRemoteDesktopAppBlocked(PolicyBlocklistService* service) {
  DCHECK(service);
  return IsURLBlocked(GURL("https://remotedesktop.google.com"), service) ||
         IsURLBlocked(GURL("https://remotedesktop.corp.google.com"), service);
}

}  // namespace utils
}  // namespace enterprise_signals
